# Format Considerations

This file records the rationale behind the checked-in
[`../.clang-format`](../.clang-format) file and the formatting constraints
inferred from the existing codebase.

The configuration is based on the current code shape in files such as:

- `src/boolean.h`
- `src/evaluate.h`
- `src/var.h`
- `src/context.h`
- `src/engine.cpp`

## Recommendation

The closest built-in `clang-format` preset appears to be `Chromium`, with a
small set of overrides for this repository.

The checked-in config should still be treated as something to validate on a
small slice before wide reformatting.

## Why `Chromium`

The current code style is broadly consistent with:

- 2-space indentation
- attached braces
- compact one-line helpers
- 4-space continuation indentation
- left-aligned pointer/reference style in declarations

`Chromium` is a better starting point than `Google`, but the repo is still not
stock `Chromium`.

## Important Repo-Specific Overrides

The current codebase strongly suggests these adjustments:

- keep namespace contents unindented to match the current repository style
- keep short functions and small blocks on one line when natural
- treat `if_as`, `if_is`, and `if_eq` as control-flow macros
- avoid automatic include sorting
- treat 80 columns as a soft target; the current tree still has a few
  intentional lines in the low-to-mid 80s

## Suggested `.clang-format`

```yaml
BasedOnStyle: Chromium

IndentWidth: 2
ContinuationIndentWidth: 4
ColumnLimit: 80

BreakBeforeBraces: Attach
PointerAlignment: Left
ReferenceAlignment: Pointer
DerivePointerAlignment: false

NamespaceIndentation: None
AlignAfterOpenBracket: DontAlign
SortIncludes: false

AllowShortFunctionsOnASingleLine: All
AllowShortBlocksOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLambdasOnASingleLine: All

SpaceBeforeParens: ControlStatementsExceptControlMacros

IfMacros:
  - if_as
  - if_is
  - if_eq
```

## Validation Advice

If the formatter config is changed, validate it first on:

- `src/boolean.h`
- `src/evaluate.h`
- `src/quantifier.h`
- `src/engine.cpp`

These files are the most sensitive because they combine:

- macro-heavy generic code
- nested lambdas
- custom control-flow macros
- line wrapping that can become noisy under the wrong preset

## Usage

The repository now provides a `format` build target when `clang-format` is
available:

```sh
cmake --build build/debug --target format
```

There is also an opt-in CMake flag:

```sh
-DTLAPP2_FORMAT_BEFORE_BUILD=ON
```

That flag is intentionally off by default. Automatically rewriting source files
during ordinary builds is usually a bad default because it makes builds mutate
the worktree and can hide unrelated formatting churn.

## Practical Guidance

- use the existing code nearby as the primary formatting reference
- prefer small manual adjustments or explicit `format` runs over hidden
  build-time rewrites
- validate any proposed `.clang-format` change on a small file set before
  touching the rest of the tree
- avoid large formatting-only diffs in macro-heavy headers
- treat this file as a rationale note, not as a substitute for the checked-in
  config
