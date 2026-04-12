# Sore

`Sore` is the simplest set-based model. It serves as the negative baseline.

## Core Idea

Each node collects a set of proposal ids and a set of nodes that have voted in
the current membership view. Once a node has votes from every node in that
view, it commits its current proposal set and broadcasts that commit.

## Local State

Each node stores:

- `status`: `Initial`, `Voted`, or `Completed`
- `nodes`: the current membership view for that node
- `voted`: which nodes have voted in that view while the node is not
  `Completed`; the field may still be present afterward but no longer drives
  transitions
- `proposals`: the set of proposal ids known locally
- `committed`: the final committed proposal set once completion happens

Messages are:

- `Vote(from, to, proposals, nodes)`
- `Commit(from, to, commit)`

## Step Rules

1. `Propose(node, id)` inserts `id` into the global proposal set and treats the
   node as having voted for `{id}`.
2. `processVote` unions incoming `proposals` into local `proposals`.
3. If the incoming `nodes` view differs from the local one, the node intersects
   memberships and clears prior votes by resetting to `Initial`.
4. The node then records votes from `source` and `self`.
5. If `voted == nodes`, the node commits its current `proposals`.
6. Otherwise, a node in `Initial` moves to `Voted` and broadcasts one `Vote`
   message carrying its current set and current node view.
7. `DeliverCommit` makes the receiver adopt the explicit `commit` payload.

## Disconnect Handling

`Disconnect(failed)` removes the failed node from `alive`, purges queued
messages to or from it, and re-runs vote processing for survivors against the
reduced membership view.

This matters because the membership change can drop votes and cause a node to
rebroadcast under the smaller node set.

## Why It Fails

The model commits as soon as every node in the current view has voted, but it
does not require those votes to confirm the same proposal set. A node can therefore
commit a union that other completed nodes do not share.

That is why `Sore` is intentionally kept as the expected failing variant.

## Counterexample Trace

One executable counterexample reaches the safety violation with this sequence:

1. `Propose(0, 10)`
2. `Propose(1, 11)`
3. `DeliverVote(0 -> 2)`
4. `DeliverVote(1 -> 2)`
5. `Disconnect(1)`
6. `DeliverVote(2 -> 0)`

The key states are:

- after step 4, node `2` has already committed `{10, 11}`
- after step 5, the live set becomes `{0, 2}`, so node `0` shrinks its local
  membership view to `{0, 2}`
- after step 6, node `0` now has votes from every node in its reduced view and
  commits only `{10}`

At that point two live completed nodes disagree:

- node `0` committed `{10}`
- node `2` committed `{10, 11}`

So the safety invariant fails.

## Why The Counterexample Happens

The flaw is that `Sore` treats these conditions as if they were equivalent:

- every node in my current view has voted
- every node in my current view has voted for the same proposal set

They are not equivalent.

Node `2` commits `{10, 11}` before the disconnect because it has heard about
both proposals. After node `1` disconnects, node `0` recomputes its membership
view as `{0, 2}` and clears prior vote progress, but it still only knows about
proposal `10`. When node `0` later receives a vote from node `2`, it sees
votes from all nodes in its current view and commits its own local proposal
set, which is still `{10}`.

So the violation is not caused by message loss or queue artifacts. It is caused
by the commit rule itself: `voted == nodes` is too weak unless the protocol also
ensures that those votes certify the same proposal set.

Recent checks:

- executable safety fails as expected in under `0.1` seconds
- TLC safety fails as expected in about `1.1` seconds (`9899` generated,
  `6762` distinct)
