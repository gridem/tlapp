# Leaderless Consensus Algorithm Notes

These notes explain the current finite models in
[`leaderless_consensus/`](../../leaderless_consensus/) at the algorithm level.
They are meant to sit between the short status note in
[`../leaderless-consensus.md`](../leaderless-consensus.md) and the source code.

## Common Model Frame

- `Nodes = {0, 1, 2}`.
- `MessageIds = {10, 11, 12}`.
- The executable model and the TLA+ specs explore all enabled transitions
  nondeterministically.
- Queue contents are part of the state, so different in-flight messages produce
  different reachable states.
- `Sore`, `Calm`, `Flat`, and `Most` include `Disconnect`.
- `Rush` currently omits `Disconnect` while the ordering model is being reduced.

## Variants

- [`sore.md`](sore.md): naive set-based voting baseline
- [`calm.md`](calm.md): unanimous carry convergence before commit
- [`flat.md`](flat.md): merge-based carry and vote propagation
- [`most.md`](most.md): majority voting per carry
- [`rush.md`](rush.md): ordered prefix commitment with generations and promises
