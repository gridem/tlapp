# Leaderless Consensus Algorithm Notes

These notes explain the current finite models in
[`leaderless_consensus/`](../../leaderless_consensus/) at the algorithm level.
They are meant to sit between the short status note in
[`../leaderless-consensus.md`](../leaderless-consensus.md) and the source code.

Additional narrative material:

- [`../article/leaderless-consensus.md`](../article/leaderless-consensus.md):
  Markdown article describing the algorithm family, verification shape, and
  lessons learned
- [`../article/leaderless-consensus.html`](../article/leaderless-consensus.html):
  HTML blog-post version of the same article
- [`../article/leaderless-consensus-short.md`](../article/leaderless-consensus-short.md):
  short professional post

## Common Model Frame

- `Nodes = {0, 1, 2}`.
- `MessageIds = {10, 11, 12}`.
- The executable model and the TLA+ specs explore all enabled transitions
  nondeterministically.
- Queue contents are part of the state, so different in-flight messages produce
  different reachable states.
- `Sore`, `Calm`, `Flat`, and `Most` include `Disconnect`.
- `Rush` keeps disconnect out of the safety model, but the liveness model
  includes one majority-preserving disconnect in the 3-node case.

## Variants

- [`sore.md`](sore.md): naive set-based voting baseline
- [`calm.md`](calm.md): unanimous proposal convergence before commit
- [`flat.md`](flat.md): merge-based proposal and vote propagation
- [`most.md`](most.md): majority voting per proposal
- [`rush.md`](rush.md): ordered prefix commitment with generations and promises
