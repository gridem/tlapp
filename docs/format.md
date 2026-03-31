# Format Considerations

This file records the current formatting considerations inferred from the
existing codebase.

There is no authoritative formatter config in the repository yet, and
`clang-format` was not available in the local shell when these notes were
written. The guidance below is therefore based on the current code shape in
files such as:

- `src/boolean.h`
- `src/evaluate.h`
- `src/var.h`
- `src/context.h`
- `src/engine.cpp`

## Recommendation

The closest built-in `clang-format` preset appears to be `Chromium`, with a
small set of overrides for this repository.

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

- indent namespace contents
- keep short functions and small blocks on one line when natural
- treat `if_as`, `if_is`, and `if_eq` as control-flow macros
- avoid automatic include sorting
- keep line wrapping conservative around 80 columns

## Suggested `.clang-format`

```yaml
BasedOnStyle: Chromium

IndentWidth: 2
ContinuationIndentWidth: 4
ColumnLimit: 80

BreakBeforeBraces: Attach
PointerAlignment: Left
DerivePointerAlignment: false

NamespaceIndentation: All
AlignAfterOpenBracket: DontAlign
SortIncludes: false

AllowShortFunctionsOnASingleLine: All
AllowShortBlocksOnASingleLine: Always
AllowShortIfStatementsOnASingleLine: AllIfsAndElse
AllowShortLambdasOnASingleLine: All

IfMacros:
  - if_as
  - if_is
  - if_eq
```

## Validation Advice

If a repo-wide formatter config is introduced, validate it first on:

- `src/boolean.h`
- `src/evaluate.h`
- `src/quantifier.h`

These files are the most sensitive because they combine:

- macro-heavy generic code
- nested lambdas
- custom control-flow macros
- line wrapping that can become noisy under the wrong preset

## Practical Guidance

Until a checked-in formatter config exists:

- use the existing code nearby as the primary formatting reference
- prefer small manual adjustments over wide reformatting
- avoid large formatting-only diffs in macro-heavy headers
- treat this file as a starting point, not a strict guarantee of exact output
