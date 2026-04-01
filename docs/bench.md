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

## Quantifier And OR Tuning Results

The next round of work applied the planned low-risk quantifier improvements and
one targeted `||` builder cleanup:

- `quantifierOp(...)` and `filterOp(...)` now bind the extracted set as a
  reference instead of copying the whole container per evaluation.
- `forall` / `exists` now use a direct bool loop when the predicate result type
  is plain `bool`.
- Assignment-producing quantifiers now combine `LogicResult` directly instead of
  routing every element through the generic `orOp` / `andOp` reducer.
- `concatVectors(...)` now appends a single-branch `LogicResult` with
  `push_back(...)` instead of exact-reserve plus `insert(...)` on every step of
  a wide left-deep `||` chain.
- When the appended `LogicResult` is a temporary, the append path now moves its
  `BranchResult`s instead of copying them.

The same benchmark commands were rerun 5 times on 2026-04-01 and the median
`per_iter_us` was recorded again.

| Benchmark | Previous latest `per_iter_us` | Quantifier and OR tuning `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 5.470 | 5.099 | -0.371 | -6.8 |
| `boolean_and_cross_16x16x16` | 70.107 | 66.736 | -3.371 | -4.8 |
| `quant_forall_late_fail_4096` | 2.447 | 2.184 | -0.263 | -10.7 |
| `quant_exists_early_hit_4096` | 0.362 | 0.005 | -0.357 | -98.6 |
| `quant_exists_late_hit_4096` | 2.493 | 2.143 | -0.350 | -14.0 |
| `quant_exists_assign_16` | 1.407 | 0.822 | -0.585 | -41.6 |

## Quantifier And OR Tuning Notes

- The container-copy removal and bool fast path are both low-risk changes that
  produced clear wins on the quantifier benchmarks.
- `quant_exists_assign_16` is now faster than the original baseline, which
  confirms that the direct branch-combining path is better than routing the
  quantifier through generic boolean reduction.
- `boolean_or_assign_64` improved again and is now much closer to the original
  closure-based baseline, but it is still a bit slower.
- `quant_exists_early_hit_4096` is now so cheap that the benchmark is
  compiler-sensitive. The current number should be read as “effectively near
  zero” rather than as a precise stable microsecond cost.

## Baseline vs Current

| Benchmark | Original baseline `per_iter_us` | Current `per_iter_us` | Delta `us` | Delta `%` |
| --- | ---: | ---: | ---: | ---: |
| `boolean_or_assign_64` | 4.469 | 5.099 | +0.630 | +14.1 |
| `boolean_and_cross_16x16x16` | 272.536 | 66.736 | -205.800 | -75.5 |
| `quant_forall_late_fail_4096` | 2.832 | 2.184 | -0.648 | -22.9 |
| `quant_exists_early_hit_4096` | 0.337 | 0.005 | -0.332 | -98.5 |
| `quant_exists_late_hit_4096` | 2.531 | 2.143 | -0.388 | -15.3 |
| `quant_exists_assign_16` | 1.169 | 0.822 | -0.347 | -29.7 |

## Liveness Benchmarks

The liveness benchmarks live in
[`samples/liveness_perf.cpp`](/Users/gridem/Documents/repo/tlapp2/samples/liveness_perf.cpp).

Unlike the boolean and quantifier microbenchmarks, these scenarios:

- build the reachable graph once
- then benchmark repeated `Engine::checkLiveness()` calls on that fixed graph

This isolates the current liveness analysis cost from one-time graph
exploration.

### Build And Run

```sh
cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DGTest_DIR=/opt/homebrew/opt/googletest/lib/cmake/GTest
cmake --build build/rel --target liveness_perf
GLOG_minloglevel=1 ./build/rel/samples/liveness_perf --gtest_brief=1
```

For the table below, the executable was run 5 times on 2026-04-01 and the
median `per_iter_us` was recorded.

### Scenarios

| Benchmark | Scenario |
| --- | --- |
| `liveness_eventually_ring_4096` | `<>(node == 2048)` on a 4096-state deterministic ring, which exercises node-predicate caching plus SCC eventuality checking |
| `liveness_wf_cycle_1024` | `WF(cycle)` on a 1024-state cycle with extra exit edges to a sink, which exercises fairness enabledness and in-SCC action-hit checks |
| `liveness_sf_cycle_1024` | `SF(cycle)` on the same graph shape, isolating the strong-fairness check path |

### Results

| Benchmark | Iterations | Checksum | Median `per_iter_us` |
| --- | ---: | ---: | ---: |
| `liveness_eventually_ring_4096` | 200 | 1638600 | 574.156 |
| `liveness_wf_cycle_1024` | 25 | 76875 | 44375.570 |
| `liveness_sf_cycle_1024` | 25 | 76875 | 44592.718 |

### Liveness Notes

- The eventuality benchmark is comparatively cheap because it only needs one
  cached predicate bit per node plus bad-subgraph cycle detection.
- The fairness benchmarks are much more expensive because each fairness clause
  still matches a large action formula against every admitted node and
  reconstructs matching targets from the graph.
- The current fairness implementation now uses bitset-style SCC summaries, but
  these single-clause benchmarks do not benefit much from that change because
  the dominant cost is still building the per-clause action caches.
- The latest cache-build path still avoids per-node hash sets by using reusable
  dense stamp arrays during target reconstruction.
- These numbers justify the next optimization step already noted in
  [`plan/liveness.md`](/Users/gridem/Documents/repo/tlapp2/plan/liveness.md):
  sharing action-hit information directly from exploration instead of
  recomputing it during the liveness pass.
