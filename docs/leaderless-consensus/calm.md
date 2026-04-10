# Calm

`Calm` tightens `Sore` by distinguishing between "ready to vote", "may commit",
and "cannot commit" after conflicting evidence arrives.

## Core Idea

A node may commit only if it has completed a full round over its current
membership view and its carry set stayed stable while it was in the
commit-eligible phase.

## Local State

Each node stores:

- `status`: `ToVote`, `MayCommit`, `CannotCommit`, or `Completed`
- `nodes`: the current membership view
- `voted`: nodes heard from in that view
- `carries`: the locally known proposal ids
- `committed`: the final committed carry set once complete

Messages are:

- `Vote(from, to, carries, nodes)`
- `Commit(from, to, commit)`

## Step Rules

1. `Apply(node, id)` inserts `id` into global `applied` and feeds it through
   `processVote` as a self-vote.
2. `processVote` first rejects input from a source that is no longer in the
   local node view.
3. If the node was already in `MayCommit` and the incoming `carries` differ
   from the local `carries`, the status drops to `CannotCommit`.
4. If the incoming node view differs, the node intersects memberships and
   votes, and a node that was in `MayCommit` also drops to `CannotCommit`.
5. After recording `source` and `self` as having voted, two cases matter:
   - if `voted == nodes` and status is still `MayCommit`, the node commits
   - if `voted == nodes` but status is `CannotCommit`, the node resets to
     `ToVote` and starts another round
6. Whenever a node is in `ToVote`, it moves to `MayCommit` and broadcasts one
   vote message with its current `carries` and `nodes`.
7. `Commit` carries the explicit committed set, and the receiver accepts it only
   if its current `carries` already match that payload.

## Disconnect Handling

`Disconnect` removes the failed node from the live set, purges its queued
messages, and replays each survivor's current `carries` against the reduced
membership view. That can force a `MayCommit` node into `CannotCommit`.

## Safety and Liveness

Safety requires all live completed nodes to have the same committed set.

The current model also checks liveness: under weak fairness of `Next`, the
system must eventually become quiescent, meaning no vote or commit messages
remain and no fresh `Apply` is still enabled.
