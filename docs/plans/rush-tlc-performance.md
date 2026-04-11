# Rush TLC Performance Plan

## Goal

Make `leaderless_consensus/specs/Rush.tla` practical for TLC, especially for
the liveness run, without changing the executable TLA++ model behavior.

## Current Status

- executable TLA++ `Rush` is already in good shape:
  - safety: about `12.9s`
  - liveness: about `22.3s`
- TLC `Rush` safety and liveness still lag far behind the executable model
- the main issue is not basic parsing or initialization; it is the cost of
  evaluating the TLA+ operators used inside `Rush.tla`

## Baseline Observations

Recent TLC runs on this machine:

- safety with the current checked-in `Rush.tla`:
  - started cleanly
  - after about `6` minutes reached only `1124` generated / `878` distinct
  - then effectively stalled with `623` states still queued
- liveness with the current checked-in `Rush.tla`:
  - started cleanly
  - stayed CPU-active
  - did not complete in the observation window

This means TLC is spending too much time per state, not just exploring a large
state graph.

## Tested Candidate Change

One spec-only optimization was tested locally but is intentionally not kept in
the checked-in `Rush.tla` yet.

The tested change did two things:

1. Rewrote `NormalizePromises` so it iterated only over actual promise prefixes
   already present in `promises`, instead of ranging over all `PromiseState`.
2. Replaced broad hot-path type-membership checks such as `core \in CoreState`
   and `stateMsgs \subseteq StateMessage` with explicit field checks.

Observed result:

- safety TLC completed successfully in about `24.1s`
- safety stats:
  - `522040` generated
  - `212530` distinct
  - depth `18`
- liveness TLC started cleanly and remained CPU-active, but it was still much
  slower than safety and was interrupted before completion

Conclusion:

- this candidate change is strong enough to fix safety tractability
- it is not sufficient by itself to make liveness fast enough

## Likely Hotspots

The main expensive parts of the current spec are:

1. `NormalizePromises`
   - broad comprehension over `PromiseState`
   - repeated `PromiseVotesFor` and `PrefixSupport` calls
2. `CoreWellFormed` and `TypeOK`
   - broad type-membership checks in the invariant path
3. `Iterate`
   - repeated record rebuilding
   - repeated promise normalization
4. sorting and sequence operators
   - `SortedSeqFromSet`
   - `SortFrom`
   - `SeqLess`

## Plan

1. Apply the tested safety optimization again in a temporary branch and rerun:
   - safety TLC
   - liveness TLC
2. If safety remains fast, keep that change as the first committed reduction.
3. Then target liveness specifically:
   - inspect whether the cost is in state-graph generation or temporal analysis
   - if the state graph is already cheap, focus on fairness/property structure
4. Reduce repeated normalization inside `Iterate`:
   - normalize only when the underlying promise set or supporting sequences
     changed
5. Revisit the liveness property shape:
   - keep semantics aligned with the executable model
   - avoid avoidable temporal-analysis overhead in TLC
6. After each step, compare:
   - TLC safety wall time
   - TLC liveness wall time
   - generated / distinct states
   - whether the executable and spec still match behaviorally

## Acceptance Criteria

- safety TLC should complete comfortably on the current 3-node / 3-message
  model
- liveness TLC should complete in a practical time window on the same model
- no semantic drift from the executable `Rush` model
