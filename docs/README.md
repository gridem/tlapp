# Documentation

This directory contains long-form project notes, reference material, and
design plans.

## Reference Notes

- [`bench.md`](bench.md): benchmark baselines, comparisons, and performance
  notes
- [`format.md`](format.md): formatting guidance inferred from the current code
  style
- [`leaderless-consensus.md`](leaderless-consensus.md): overview and status of
  the leaderless consensus models

## Design Plans

Planning documents now live under [`plans/`](plans/):

- [`plans/boolean-optimization.md`](plans/boolean-optimization.md)
- [`plans/liveness.md`](plans/liveness.md)
- [`plans/partial-order-reduction.md`](plans/partial-order-reduction.md)

## Scope Recommendation

Keep `docs/` for durable narrative material:

- architecture notes
- benchmark records
- design plans

Keep root-level Markdown limited to repo-entry documents such as
[`README.md`](../README.md) and [`TODO.md`](../TODO.md).
