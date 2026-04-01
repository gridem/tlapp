# Benchmark Results

This file records the baseline for the boolean and quantifier benchmarks and
the measured results after each major refactor phase.

## Environment

- Date: 2026-03-31
- Machine: Apple M1 Max
- Compiler: Apple clang 17.0.0
- Build type: `RelWithDebInfo`
- Generator: `Ninja`

## Build

The benchmarks are built as normal `samples/` executables.

Commands used for this baseline:

```sh
cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DGTest_DIR=/opt/homebrew/opt/googletest/lib/cmake/GTest
cmake --build build/rel --target boolean_perf quantifier_perf
```

Run commands:

```sh
./build/rel/samples/boolean_perf --gtest_brief=1
./build/rel/samples/quantifier_perf --gtest_brief=1
```

For the table below, each benchmark executable was run 5 times and the median
`per_iter_us` was recorded.

## Scenarios

| Benchmark | Scenario |
| --- | --- |
| `boolean_or_assign_64` | 64-way `\|\|` over assignment-producing clauses such as `x == i` |
| `boolean_and_cross_16x16x16` | `(x == ...) && (y == ...) && (z == ...)` with 16 branches each, forcing eager branch cross-product materialization |
| `quant_forall_late_fail_4096` | `forall(vec, i != 4095)` over a 4096-element context vector, failing only on the last element |
| `quant_exists_early_hit_4096` | `exists(vec, i == 0)` over a 4096-element context vector, succeeding on the first element |
| `quant_exists_late_hit_4096` | `exists(vec, i == 4095)` over a 4096-element context vector, succeeding on the last element |
| `quant_exists_assign_16` | `exists({1..16}, x == i)` where the predicate produces assignment branches instead of a plain bool |

## Baseline Results

| Benchmark | Iterations | Checksum | Median `per_iter_us` |
| --- | ---: | ---: | ---: |
| `boolean_or_assign_64` | 5000 | 320000 | 4.469 |
| `boolean_and_cross_16x16x16` | 300 | 1228800 | 272.536 |
| `quant_forall_late_fail_4096` | 2000 | 0 | 2.832 |
| `quant_exists_early_hit_4096` | 5000 | 5000 | 0.337 |
| `quant_exists_late_hit_4096` | 2000 | 2000 | 2.531 |
| `quant_exists_assign_16` | 5000 | 80000 | 1.169 |

## Notes

- `boolean_and_cross_16x16x16` is the clearest current stress case for eager
  branch multiplication. It should be the primary before/after check for
  boolean normalization work.
- `quant_exists_early_hit_4096` vs `quant_exists_late_hit_4096` isolates how
  much the current quantifier path benefits from early termination.
- `quant_exists_assign_16` is the main quantifier benchmark for assignment-style
  predicates and should be rerun after any quantifier lowering or binding
  changes.
- Future optimization results should be compared against this table using the
  same commands, build type, and benchmark inputs.

## Phase 2 Results

Phase 2 replaced closure-based branch composition with explicit branch data in
[`boolean.h`](/Users/gridem/.codex/worktrees/4565/tlapp2/src/boolean.h). The
same benchmark commands were rerun 5 times on 2026-03-31 and the median
`per_iter_us` was recorded again.

| Benchmark | Baseline `per_iter_us` | Phase 2 `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 4.469 | 15.412 | +10.943 | +244.9 |
| `boolean_and_cross_16x16x16` | 272.536 | 296.473 | +23.937 | +8.8 |
| `quant_forall_late_fail_4096` | 2.832 | 2.358 | -0.474 | -16.7 |
| `quant_exists_early_hit_4096` | 0.337 | 0.341 | +0.004 | +1.2 |
| `quant_exists_late_hit_4096` | 2.531 | 2.392 | -0.139 | -5.5 |
| `quant_exists_assign_16` | 1.169 | 3.702 | +2.533 | +216.7 |

## Phase 2 Notes

- Phase 2 is semantically correct and passes the full debug test suite, but it
  is not a net performance win yet.
- The worst regressions are in branch-producing cases:
  `boolean_or_assign_64` and `quant_exists_assign_16`.
- The most likely reason is the current runtime IR shape:
  `BranchResult` stores heap-allocated polymorphic ops behind
  `std::shared_ptr<const detail::IBranchOp>`, which replaces closure nesting
  with virtual dispatch plus allocation overhead.
- The next optimization work should target the branch-op storage format first,
  not the boolean algebra surface API.

## Phase 2 Cleanup Results

After the first Phase 2 landing, the branch-op storage was simplified again to
remove the virtual base class in
[`boolean.h`](/Users/gridem/.codex/worktrees/4565/tlapp2/src/boolean.h). The
current code stores shared immutable op payloads plus a typed apply function.
The same benchmark commands were rerun 5 times on 2026-03-31 and the median
`per_iter_us` was recorded again.

| Benchmark | Phase 2 `per_iter_us` | Phase 2 cleanup `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 15.412 | 16.250 | +0.838 | +5.4 |
| `boolean_and_cross_16x16x16` | 296.473 | 307.933 | +11.460 | +3.9 |
| `quant_forall_late_fail_4096` | 2.358 | 2.442 | +0.084 | +3.6 |
| `quant_exists_early_hit_4096` | 0.341 | 0.278 | -0.063 | -18.5 |
| `quant_exists_late_hit_4096` | 2.392 | 2.529 | +0.137 | +5.7 |
| `quant_exists_assign_16` | 3.702 | 3.860 | +0.158 | +4.3 |

## Phase 2 Cleanup Notes

- This cleanup improves code shape more than raw runtime.
- Replacing the virtual branch-op hierarchy with a typed function-pointer model
  did not materially recover the branch-heavy regression.
- That suggests the dominant remaining cost is not virtual dispatch alone. The
  bigger issue is still branch materialization plus per-op allocation/copying.
- The next useful optimization target is higher level:
  reduce branch creation and copying, or delay branch product expansion.

## Phase 2 Branch Tuning Results

The next round of Phase 2 work kept the explicit branch IR, but targeted the
actual hot allocations:

- `BranchResult` no longer uses a heap-backed `std::vector<BranchOp>` for the
  common tiny-branch cases. It now keeps up to 4 ops inline before spilling.
- `mulVectorsImpl(...)` now builds result branches in place instead of creating
  a temporary concatenated branch and then moving it into the result vector.
- `BranchOp` kept the inline-op storage that had already recovered much of the
  per-op overhead from the first Phase 2 landing.

The same benchmark commands were rerun 5 times on 2026-04-01 and the median
`per_iter_us` was recorded again.

| Benchmark | Phase 2 cleanup `per_iter_us` | Phase 2 branch tuning `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 16.250 | 5.470 | -10.780 | -66.3 |
| `boolean_and_cross_16x16x16` | 307.933 | 70.107 | -237.826 | -77.2 |
| `quant_forall_late_fail_4096` | 2.442 | 2.447 | +0.005 | +0.2 |
| `quant_exists_early_hit_4096` | 0.278 | 0.362 | +0.084 | +30.2 |
| `quant_exists_late_hit_4096` | 2.529 | 2.493 | -0.036 | -1.4 |
| `quant_exists_assign_16` | 3.860 | 1.407 | -2.453 | -63.5 |

## Phase 2 Branch Tuning Notes

- This is the first explicit-branch representation that is clearly better than
  the previous Phase 2 versions on the branch-heavy workloads.
- The biggest win came from removing heap allocation per tiny branch, not from
  further shrinking the branch-op metadata.
- `boolean_and_cross_16x16x16` is now substantially faster than the original
  baseline because the in-place cross-product build avoids an extra temporary
  branch move for every branch pair.
- `quant_exists_assign_16` is now close to the original closure-based baseline,
  which supports the claim that assignment-producing quantifiers are primarily
  paying branch materialization cost.
- `boolean_or_assign_64` is much better than the earlier explicit-branch
  versions, but is still above the original baseline. The remaining cost is
  likely in repeated `LogicResult` growth along wide `||` chains.
- `quant_exists_early_hit_4096` regressed slightly. That path is dominated by
  plain predicate evaluation, so the next gains there should come from the
  planned quantifier fast path rather than more boolean-branch tuning.

## Baseline vs Latest

This table compares the original closure-based baseline directly against the
latest Phase 2 branch-tuned implementation.

| Benchmark | Original baseline `per_iter_us` | Latest `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 4.469 | 5.470 | +1.001 | +22.4 |
| `boolean_and_cross_16x16x16` | 272.536 | 70.107 | -202.429 | -74.3 |
| `quant_forall_late_fail_4096` | 2.832 | 2.447 | -0.385 | -13.6 |
| `quant_exists_early_hit_4096` | 0.337 | 0.362 | +0.025 | +7.4 |
| `quant_exists_late_hit_4096` | 2.531 | 2.493 | -0.038 | -1.5 |
| `quant_exists_assign_16` | 1.169 | 1.407 | +0.238 | +20.4 |

## Summary

- The latest explicit-branch implementation is a large win for branch
  cross-product construction and a modest win for late-fail / late-hit
  quantifier scans.
- The remaining regressions are concentrated in wide assignment-producing `||`
  chains and assignment-producing quantifiers.
- That suggests the next optimization pass should focus on `LogicResult` growth
  along `||` chains and on a pure-bool quantifier fast path, not another major
  redesign of branch storage.
