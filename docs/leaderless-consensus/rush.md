# Rush

`Rush` is the most advanced variant in this set. Instead of committing an
unordered set, it commits a sequence and only requires committed sequences to
stay prefix-compatible.

## Core Idea

Each node tracks one ordered uncommitted suffix per node, plus prefix promises.
Commitment grows one position at a time when there is quorum support for the
same prefix.

## Local State

Each node stores:

- `core.proposals`: the unordered set of known uncommitted proposal ids
- `core.nodesMessages`: one `messages + generation` suffix per node, relative to `committed`
- `core.promises`: promise votes keyed by uncommitted prefix candidates
- `committed`: the local committed prefix

Messages are:

- `State(from, to, core, committed)`

## Key Terms

- `prefix`: a candidate committed sequence such as `[10]` or `[10, 11]`
- `support(prefix)`: who currently observes the prefix; the set of nodes whose
  current message sequence starts with `prefix`
- `promise.votes(prefix)`: who has explicitly contributed commit evidence for
  that prefix
- `committed`: the longest prefix this node has already finalized locally

## Step Rules

1. `Propose(node, id)` is allowed only while that node is still in the exact
   initial local state.
2. Proposing a proposal id creates an incoming core with that one new proposal and
   merges it into the receiver's local core.
3. `mergeState` adopts newer per-node message sequences using:
   - higher generation first
   - lexicographically larger sequence as an equal-generation tie-break
4. Newly learned proposal ids are appended to the receiver's own sequence, and
   that local sequence generation is advanced.
5. Starting from the current uncommitted suffix, the node scans positions:
   - if a quorum of node sequences has the same id at this position, that id is
     appended to the candidate prefix
   - support for that prefix is recomputed from `nodesMessages`
   - promise votes are stored by `prefix` only and intersected with current
     support
   - if both support size and vote count reach quorum, the node can extend
     `committed`
6. After `committed` grows, the node rebases `core` so it keeps only the remaining uncommitted suffix state. Committed ids are removed from `core.proposals`, from the stored per-node suffixes, and from promise prefixes.
7. If there is no majority id at the current position, the node sorts its own
   remaining suffix once. That canonicalizes the local tail and may unlock
   later majority checks.
8. A local `stabilize` step reruns the merge logic against an empty incoming core.
   That allows a node to sort or finalize its own suffix even when no new
   message arrives.
9. Whenever local state changes, the node broadcasts `(core, committed)` to
   peers. Pending messages are coalesced so there is at most one in-flight
   state message per `(from, to)` pair.

## Current Reductions

Compared with earlier drafts, the current model also:

- stores promises by `prefix` only, not by `(prefix, support)`
- keeps `core` as the uncommitted suffix relative to `committed`
- carries `committed` in every state message
- normalizes promises after merges and after suffix sorting
- uses bounded generations derived from the finite message space
- keeps the safety model focused on proposal, state-message delivery, and local stabilization
- adds a separate liveness model that allows one majority-preserving disconnect in the 3-node case

These changes are aimed at keeping the model finite and reducing avoidable
state churn. `Rush` still does not rely on timeout-based failure handling. That
gives it a structural robustness advantage over timeout-driven designs:
progress is not gated on waiting for timeout expiry. It is reasonable to expect
better tail-latency behavior from that design choice, but the checked models do
not prove an absolute p99 bound.

## Safety Shape

`Rush` does not check equality of committed values. Instead it checks that every
pair of committed sequences is prefix-comparable. That matches the intended
progressive-commit structure: later commits may extend earlier ones, but they
must not branch.

The implemented safety checks also require:

- queued state-message endpoints stay live
- every proposal id in local cores, state messages, promise prefixes, and
  committed prefixes stays within the global `proposed` set
- committed ids do not remain in `core.proposals`
- local sequences and promise prefixes contain no duplicates
- promise votes stay within `support(prefix)`

The current executable model also carries a liveness check in a separate
liveness model.

- In the executable TLA++ model, the liveness form is
  `weakFairness(proposeAny()) && weakFairness(deliverAnyState()) &&
  weakFairness(stabilizeAny()) &&
  eventually(commitHappenedExpr(state))`.
- `proposeAny()` means there exists some node and some proposal id such that a
  `Propose(node, id)` step is currently enabled and can be taken.
- `weakFairness(proposeAny())` means proposal work cannot stay continuously
  enabled forever without eventually taking a propose step.
- `deliverAnyState()` means there exists some in-flight state message such that
  a `DeliverState(msg)` step is currently enabled and can be taken.
- `weakFairness(deliverAnyState())` means state delivery cannot stay
  continuously enabled forever without eventually taking a state-delivery step.
- `stabilizeAny()` means there exists some alive node whose local suffix state
  can still be advanced by rerunning the merge logic against an empty incoming
  core.
- `weakFairness(stabilizeAny())` means local stabilization work cannot stay
  continuously enabled forever without eventually taking a stabilize step.
- `commitHappenedExpr(state)` means every alive node has committed a non-empty prefix.
- `eventually(commitHappenedExpr(state))` means that commit condition must
  eventually become true.
- The liveness-only failure model additionally allows one disconnect while a
  majority remains alive. In the 3-node configuration used here, that means one
  node may fail and the remaining two must both still reach a non-empty commit.

Recent `build/rel` runs:

- executable safety model: about 25.0 seconds, `353048` states, `1460128` transitions
- executable liveness model with one majority-preserving disconnect:
  about 77.8 seconds, `723104` states, `3060328` transitions
- TLC safety: in progress; a recent safety-only run started cleanly but remained
  slow after `60` seconds (`372` generated, `270` distinct)
- TLC liveness: in progress; the current `LiveSpec` now parses and starts
  cleanly, but did not complete in the recent run window
