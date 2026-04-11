# Calm

`Calm` tightens `Sore` by distinguishing between "ready to vote", "may commit",
and "cannot commit" after conflicting evidence arrives.

## Core Idea

A node may commit only if it has completed a full pass over its current
membership view and its proposal set stayed stable while it was in the
commit-eligible phase.

## Local State

Each node stores:

- `status`: `ToVote`, `MayCommit`, `CannotCommit`, or `Completed`
- `nodes`: the current membership view
- `voted`: nodes heard from in that view while the node is not `Completed`; the
  field may still be present afterward but no longer drives transitions
- `proposals`: the locally known proposal ids
- `committed`: the final committed proposal set once complete

Messages are:

- `Vote(from, to, proposals, nodes)`
- `Commit(from, to, commit)`

## Step Rules

1. `Propose(node, id)` inserts `id` into the global proposal set and feeds it
   through `processVote` as a self-vote.
2. `processVote` first rejects input from a source that is no longer in the
   local node view.
3. If the node was already in `MayCommit` and the incoming `proposals` differ
   from the local `proposals`, the status drops to `CannotCommit`.
4. If the incoming node view differs, the node intersects memberships and
   votes, and a node that was in `MayCommit` also drops to `CannotCommit`.
5. After recording `source` and `self` as having voted, two cases matter:
   - if `voted == nodes` and status is still `MayCommit`, the node commits
   - if `voted == nodes` but status is `CannotCommit`, the node resets to
     `ToVote` and starts another pass over its current membership view
6. Whenever a node is in `ToVote`, it moves to `MayCommit` and broadcasts one
   vote message with its current `proposals` and `nodes`.
7. `Commit` carries the explicit committed set, and the receiver accepts it only
   if its current `proposals` already match that payload.

## Disconnect Handling

`Disconnect` removes the failed node from the live set, purges its queued
messages, and replays each survivor's current `proposals` against the reduced
membership view. That can force a `MayCommit` node into `CannotCommit`.
The safety model keeps unrestricted disconnects. The liveness model reuses the
same transition logic, but limits disconnects so that a majority of the
original nodes remains alive. In the 3-node model, that means at most one
disconnect.

## Safety and Liveness

Safety requires all live completed nodes to have the same committed proposal set.

The current model also checks liveness: under weak fairness of `ProposeAny`
and `DeliverAnyVote`, some node must eventually commit a non-empty proposal
set. That liveness check is evaluated in a separate liveness model that uses
the majority-preserving disconnect rule above.

Recent `build/rel` runs:

- executable safety model: about 3.6 seconds, `234780` states, `1367893` transitions
- executable liveness model: about 5.5 seconds, `214265` states, `1239748` transitions
- TLC safety: about 8.4 seconds, `568552` generated, `149402` distinct
- TLC liveness: about 22.6 seconds, `462925` generated, `128887` distinct
