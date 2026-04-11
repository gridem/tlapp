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

Recent checks:

- TLC safety fails as expected in about `1.1` seconds (`9899` generated,
  `6762` distinct)
