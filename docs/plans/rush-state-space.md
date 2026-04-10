# Rush State-Space Plan

## Goal

Reduce `Rush` state explosion without changing the intended safety model.

## Priority

1. Coalesce pending `stateMsgs` to at most one message per `(from, to)` edge.
2. Canonicalize promises to `prefix -> votes`; derive support from `nodesMessages`.
3. Fix generation semantics:
   - advance on every semantic `nodesMessages[self]` change
   - avoid the current weak fixed cap behavior
   - define a deterministic merge rule for equal generations
4. Decide failure handling explicitly:
   - either model membership/quorum changes on disconnect
   - or remove `Disconnect` from `Rush` until membership is modeled
5. Canonicalize the uncommitted tail earlier to remove equivalent permutations.
6. Consider explicit commit-prefix dissemination to collapse old evidence.

## TLA+ Alignment

1. Replace total `PromiseMap` with a sparse representation.
2. Remove stored `support` from promise keys.
3. Restrict promise prefixes to reachable message prefixes, not `AllMessageSeqs`.
4. Make sorting semantics match the C++ model.

## Verification

1. Re-run the executable `Rush` sample in `build/rel`.
2. Track state growth before and after each reduction.
3. Re-run TLC only after the C++ model stabilizes.
