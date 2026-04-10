# Rush

`Rush` is the ordering-oriented variant. Instead of committing an unordered set,
it commits a sequence and only requires committed sequences to stay
prefix-compatible.

## Core Idea

Each node tracks one ordered message sequence per node, plus prefix promises.
Commitment grows one position at a time when there is quorum support for the
same prefix.

## Local State

Each node stores:

- `core.carries`: the unordered set of known proposal ids
- `core.nodesMessages`: one `messages + generation` entry per node
- `core.promises`: promise votes keyed by committed prefix candidate
- `committed`: the local committed prefix

Messages are:

- `State(from, to, core)`

## Step Rules

1. `Apply(node, id)` is allowed only while that node is still in the exact
   initial local state.
2. Applying a proposal creates an incoming core with that one new carry and
   merges it into the receiver's local core.
3. `mergeState` adopts newer per-node message sequences using:
   - higher generation first
   - lexicographically larger sequence as an equal-generation tie-break
4. Newly learned carry ids are appended to the receiver's own sequence, and
   that local sequence generation is advanced.
5. Starting from the current committed prefix length, the node scans positions:
   - if a quorum of node sequences has the same id at this position, that id is
     appended to the candidate prefix
   - support for that prefix is recomputed from `nodesMessages`
   - promise votes are stored by `prefix` only and intersected with current
     support
   - if both support size and vote count reach quorum, the node can extend
     `committed`
6. If there is no majority id at the current position, the node sorts its own
   remaining suffix once. That canonicalizes the local tail and may unlock
   later majority checks.
7. Whenever local core state changes, the node broadcasts its full core to peers.
   Pending messages are coalesced so there is at most one in-flight state
   message per `(from, to)` pair.

## Current Reductions

Compared with earlier drafts, the current model also:

- stores promises by `prefix` only, not by `(prefix, support)`
- normalizes promises after merges and after suffix sorting
- uses bounded generations derived from the finite message space
- omits `Disconnect` for now

These changes are aimed at keeping the model finite and reducing avoidable
state churn.

## Safety Shape

`Rush` does not check equality of committed values. Instead it checks that every
pair of committed sequences is prefix-comparable. That matches the intended
progressive-commit structure: later commits may extend earlier ones, but they
must not branch.
