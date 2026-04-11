# Flat

`Flat` keeps set-based proposals, but it also propagates vote evidence in each
vote message instead of rebuilding that evidence only from local state.

## Core Idea

Nodes merge proposal sets and the sets of nodes counted as supporting each
proposal. When the merged voting set covers the merged node set, the node
commits its current proposal set.

## Local State

Each node stores:

- `status`: `Voting` or `Committed`
- `nodes`: the current membership view
- `votes`: nodes whose votes are currently counted during voting
- `proposals`: the locally known proposal ids
- `committed`: the final committed proposal set once complete

Messages are:

- `Vote(from, to, proposals, nodes, votes)`
- pending commit recipients, keyed only by `to` in the current model

## Step Rules

1. In the current reduced model, `Propose` is limited to one fixed proposal per
   node:
   - node `0` may propose `10`
   - node `1` may propose `11`
   - node `2` may propose `12`
2. `processVote` intersects the incoming and local node views, and unions the
   incoming and local proposal sets.
3. Vote preservation is conditional:
   - incoming `votes` are kept only if the incoming `(nodes, proposals)` already
     match the merged result
   - local `votes` are kept only if the local `(nodes, proposals)` already match
     the merged result
4. The merged vote set always includes `self`, then gets intersected with the
   merged node set.
5. If `votes == nodes`, the node commits.
6. Otherwise it broadcasts a new `Vote` message with the merged `proposals`,
   merged `nodes`, and merged `votes`.
7. `Commit` is payload-free in the current model, so the receiver commits its
   own current `proposals`. The queue representation is reduced to a recipient set
   because the sender identity does not affect the state transition.

## Disconnect Handling

`Disconnect` removes the failed node from `alive`, purges queued messages, and
replays each survivor's local `proposals` against the smaller node set.

## Nuances

- `Flat` retains more message detail than `Calm`, because vote messages carry
  `votes` in addition to `proposals` and `nodes`.
- The current model also coalesces vote messages by exact
  `(from, to, proposals, nodes)` bucket, keeping only non-dominated `votes` sets.
- Inbound vote traffic to an already committed node is purged on commit, and
  the commit queue is reduced to pending recipients instead of `(from, to)`
  pairs.
- That larger message payload is the main reason this variant has a much larger
  reachable state space.

## Safety and Liveness

Safety requires all live committed nodes to agree on the committed proposal set.

The current model also checks liveness in a separate liveness model. Safety
keeps unrestricted disconnects. Liveness reuses the same transition logic, but
limits disconnect so that a majority of the original nodes remains alive. Under
weak fairness of `ProposeAny` and `DeliverAnyVote`, some node must eventually
commit a non-empty proposal set.

Recent `build/rel` runs:

- executable safety model: about 182.4 seconds
- executable liveness model: about 296.5 seconds
- TLC safety: about 46.7 seconds, `1396276` generated, `224973` distinct
- TLC liveness: about 161.3 seconds, `1331224` generated, `216323` distinct
