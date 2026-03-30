# TODO

## High priority
- [ ] Replace `std` specializations in `src/std.h` with project-local hash/equal functors; avoid undefined behavior in `std`.

## Medium priority
- [ ] Refactor quantifier to generalize logic for both filter & quantifier (`src/functional.h:8`).

## Low priority / cleanup
- [ ] Remove or guard unused code: `ErasureExpressionVariant` in `src/expression.h`, debug macros in `src/troubleshooting.h`, and the commented block in `src/functional.h`.

## Open questions
- [ ] Should samples depend on GTest long-term, or move to plain `main()` examples to reduce dependencies?
- [ ] Is reliance on `std` specializations intentional for interoperability or just convenience?
