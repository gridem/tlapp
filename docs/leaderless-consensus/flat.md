# Flat

`Flat` keeps set-based carries, but it also propagates vote evidence in each
vote message instead of rebuilding that evidence only from local state.

## Core Idea

Nodes merge carry sets and vote sets in a uniform way. When the merged vote set
covers the merged node set, the node commits its current carry set.

## Local State

Each node stores:

- `status`: `Voting` or `Committed`
- `nodes`: the current membership view
- `votes`: nodes whose votes are currently counted
- `carries`: the locally known proposal ids
- `committed`: the final committed set once complete

Messages are:

- `Vote(from, to, carries, nodes, votes)`
- pending commit recipients, keyed only by `to` in the current model

## Step Rules

1. In the current reduced model, `Propose` is limited to one fixed proposal per
   node:
   - node `0` may propose `10`
   - node `1` may propose `11`
   - node `2` may propose `12`
2. `processVote` intersects the incoming and local node views, and unions the
   incoming and local carry sets.
3. Vote preservation is conditional:
   - incoming `votes` are kept only if the incoming `(nodes, carries)` already
     match the merged result
   - local `votes` are kept only if the local `(nodes, carries)` already match
     the merged result
4. The merged vote set always includes `self`, then gets intersected with the
   merged node set.
5. If `votes == nodes`, the node commits.
6. Otherwise it broadcasts a new `Vote` message with the merged `carries`,
   merged `nodes`, and merged `votes`.
7. `Commit` is payload-free in the current model, so the receiver commits its
   own current `carries`. The queue representation is reduced to a recipient set
   because the sender identity does not affect the state transition.

## Disconnect Handling

`Disconnect` removes the failed node from `alive`, purges queued messages, and
replays each survivor's local `carries` against the smaller node set.

## Nuances

- `Flat` retains more message detail than `Calm`, because vote messages carry
  `votes` in addition to `carries` and `nodes`.
- The current model also coalesces vote messages by exact
  `(from, to, carries, nodes)` bucket, keeping only non-dominated `votes` sets.
- Inbound vote traffic to an already committed node is purged on commit, and
  the commit queue is reduced to pending recipients instead of `(from, to)`
  pairs.
- That larger message payload is the main reason this variant has a much larger
  reachable state space.
- On the current branch tip, the reduced executable model now completes. A
  recent release run finished in about 175 seconds with `770773` states and
  `8560654` transitions.
