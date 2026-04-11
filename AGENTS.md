# AGENTS.md

## Project overview
- TLA++ is a C++20 library that models TLA+ style temporal logic via tagged expression templates and heavy macro helpers.
- Core library code lives in `src/`, tests in `tests/`, and runnable samples in `samples/`.

## Repository layout
- `src/`: main library headers and a few `.cpp` translation units (e.g. `context.cpp`, `engine.cpp`, `value.cpp`).
- `tests/`: GoogleTest-based unit tests (each `.cpp` is built into `tlapp2_tests`).
- `samples/`: small example programs; each `*.cpp` builds into a standalone executable.
- `benchmarks/`: GoogleTest-based benchmark executables plus shared benchmark helpers.
- `docs/`: long-form project notes and reference material.
- `docs/plans/`: design and implementation plans.
- `scripts/`: perf helpers that assume a local `.build/RelWithDebInfo` layout.
- `build/`: build artifacts are present in-repo; avoid editing these by hand.

## Build and test
- Requires CMake, a C++20 compiler, GoogleTest, and glog (see `CMakeLists.txt`).
- Prefer Ninja builds.
- A repo-root `.clang-format` is checked in. Prefer the explicit `format`
  target over ad hoc formatter settings.
- If you modify any `*.h` or `*.cpp` file under `src/`, `tests/`, `samples/`,
  `benchmarks/`, or `leaderless_consensus/`, run
  `cmake --build <build-dir> --target format` before any build, test, or
  benchmark command.
- For normal development and test runs, prefer a Debug build:
  - `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build/debug --target tlapp2_tests`
  - `./build/debug/tlapp2_tests` (CTest discovery is not enabled by default)
- To apply the checked-in formatter explicitly:
  - `cmake --build build/debug --target format`
- For performance work and benchmarks, prefer a separate optimized build:
  - `cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - `cmake --build build/rel`
  - Benchmark binaries are emitted under `build/rel/benchmarks/` (for example `boolean_perf`, `quantifier_perf`, `liveness_perf`, `engine_perf`)
  - Example benchmark runs:
    - `./build/rel/benchmarks/boolean_perf --gtest_brief=1`
    - `./build/rel/benchmarks/quantifier_perf --gtest_brief=1`
  - Use `RelWithDebInfo` for any performance measurement or benchmark comparison. Do not use Debug timings as benchmark results.
- If an activated Conda environment causes CMake to pick the wrong GTest package, pass `GTest_DIR` explicitly in the configure command instead of changing `CMakeLists.txt`, for example:
  - `-DGTest_DIR=<path-to-GTest-config>`
- `TLAPP2_FORMAT_BEFORE_BUILD=ON` exists as an opt-in workflow, but keep it
  off by default. Automatically rewriting sources during ordinary builds is
  usually not desirable.
- Sample binaries are emitted under the chosen build directory's `samples/` subdirectory.
- Benchmark binaries are emitted under the chosen build directory's `benchmarks/` subdirectory.

## Commits
- Follow the recent history style for commit messages: use a bracketed type prefix such as `[feat]`, `[fix]`, `[md]`, `[docs]`, or `[tests]`.
- When useful, add a short scope after the prefix, for example `[feat] Benchmarks: ...` or `[md] README: ...`.
- Prefer imperative commit subjects and align wording with the existing history before creating or amending commits.

## Coding conventions and patterns
- Indentation is 2 spaces; braces on the same line; keep headers lightweight with `#pragma once`.
- Prefer existing repo style and local patterns over alternate equivalent forms. If nearby code already uses a macro/helper pattern, follow that style instead of rewriting it into a different personal style.
- The codebase relies on macro helpers from `src/macro.h` (`tname`, `fun`, `fwd`, `lam`, `let`, `if_is`, `if_eq`, etc.). Prefer these utilities instead of rolling new template boilerplate.
- Avoid introducing ordinary `inline` free functions in headers. Prefer the existing macro/helper style, templates, or move non-template implementations to `.cpp` files instead of papering over ODR issues with `inline`.
- Tagged-expression pattern matching is central; see `src/tag.h`, `src/expression.h`, and operator overloads in `src/operation.h`/`src/infix.h` before changing operator behavior.
- For Boolean logic behavior and normalization, follow the existing patterns in `src/boolean.h` and `src/evaluate.h`.
- Keep new tests close to the feature area (e.g. add to `tests/boolean.cpp` for boolean logic changes).

## Notes
- `scripts/perf*.sh` assume a specific build output path (`.build/RelWithDebInfo`). Adjust or mirror that layout if you need to use them.
- When reporting benchmark numbers, build and run from a `RelWithDebInfo` tree and keep the build type consistent across before/after comparisons.
- When reporting model-check, sample, or benchmark runs, include the exact command, the build tree used, and the most useful final details available from the run. Prefer runtime, pass/fail status, and key engine stats such as total states, transitions, processed, queued, drain, or notable progress checkpoints when those counters are available.
- Keep environment-specific package paths out of repo CMake files; prefer passing them explicitly at configure time.
- Keep Markdown links portable. Prefer repo-relative links in `docs/` and avoid absolute filesystem paths in checked-in documentation.
