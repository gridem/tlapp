# AGENTS.md

## Project overview
- TLA++ is a C++20 library that models TLA+ style temporal logic via tagged expression templates and heavy macro helpers.
- Core library code lives in `src/`, tests in `tests/`, and runnable samples in `samples/`.

## Repository layout
- `src/`: main library headers and a few `.cpp` translation units (e.g. `context.cpp`, `engine.cpp`, `value.cpp`).
- `tests/`: GoogleTest-based unit tests (each `.cpp` is built into `tlapp2_tests`).
- `samples/`: small example programs; each `*.cpp` builds into a standalone executable.
- `scripts/`: perf helpers that assume a local `.build/RelWithDebInfo` layout.
- `build/`: build artifacts are present in-repo; avoid editing these by hand.

## Build and test
- Requires CMake, a C++20 compiler, GoogleTest, and glog (see `CMakeLists.txt`).
- Prefer Ninja builds.
- For normal development and test runs, prefer a Debug build:
  - `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build/debug --target tlapp2_tests`
  - `./build/debug/tlapp2_tests` (CTest discovery is not enabled by default)
- For performance work and benchmarks, prefer a separate optimized build:
  - `cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - `cmake --build build/rel`
- If an activated Conda environment causes CMake to pick the wrong GTest package, pass `GTest_DIR` explicitly in the configure command instead of changing `CMakeLists.txt`. On this machine, the Homebrew config path is:
  - `-DGTest_DIR=/opt/homebrew/opt/googletest/lib/cmake/GTest`
- Sample binaries are emitted under the chosen build directory's `samples/` subdirectory.

## Coding conventions and patterns
- Indentation is 2 spaces; braces on the same line; keep headers lightweight with `#pragma once`.
- The codebase relies on macro helpers from `src/macro.h` (`tname`, `fun`, `fwd`, `lam`, `let`, `if_is`, `if_eq`, etc.). Prefer these utilities instead of rolling new template boilerplate.
- Tagged-expression pattern matching is central; see `src/tag.h`, `src/expression.h`, and operator overloads in `src/operation.h`/`src/infix.h` before changing operator behavior.
- For Boolean logic behavior and normalization, follow the existing patterns in `src/boolean.h` and `src/evaluate.h`.
- Keep new tests close to the feature area (e.g. add to `tests/boolean.cpp` for boolean logic changes).

## Notes
- `scripts/perf*.sh` assume a specific build output path (`.build/RelWithDebInfo`). Adjust or mirror that layout if you need to use them.
- Keep environment-specific package paths out of repo CMake files; prefer passing them explicitly at configure time.
