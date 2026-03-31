# Benchmark Baseline

This file records the current baseline for the boolean and quantifier
benchmarks before the planned boolean/quantifier refactor work.

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

## Results

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
