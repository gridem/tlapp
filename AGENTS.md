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
- Typical build:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - `cmake --build build`
- Run tests:
  - `./build/tlapp2_tests` (CTest discovery is not enabled by default)
- Sample binaries are emitted under `build/samples/` with names matching their source files.

## Coding conventions and patterns
- Indentation is 2 spaces; braces on the same line; keep headers lightweight with `#pragma once`.
- The codebase relies on macro helpers from `src/macro.h` (`tname`, `fun`, `fwd`, `lam`, `let`, `if_is`, `if_eq`, etc.). Prefer these utilities instead of rolling new template boilerplate.
- Tagged-expression pattern matching is central; see `src/tag.h`, `src/expression.h`, and operator overloads in `src/operation.h`/`src/infix.h` before changing operator behavior.
- For Boolean logic behavior and normalization, follow the existing patterns in `src/boolean.h` and `src/evaluate.h`.
- Keep new tests close to the feature area (e.g. add to `tests/boolean.cpp` for boolean logic changes).

## Notes
- `scripts/perf*.sh` assume a specific build output path (`.build/RelWithDebInfo`). Adjust or mirror that layout if you need to use them.
