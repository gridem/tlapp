# Most

`Most` switches from unanimous agreement on the whole proposal set to majority
tracking for each proposal id.

## Core Idea

A node can commit once it has heard from every node in its current membership
view and every proposal id in its current proposal set has a strict majority of
supporters.

## Local State

Each node stores:

- `status`: `Voting` or `Committed`
- `nodes`: the current membership view
- `votes`: which nodes are currently counted as having voted in that node's
  current membership view during voting
- `proposalVotes`: for each proposal id, the set of nodes currently counted as
  supporting that proposal
- `proposals`: the locally known proposal ids
- `committed`: the final committed proposal set once complete

Messages are:

- `Vote(from, to, proposals, nodes)`
- `Commit(from, to, commit)`

## Step Rules

1. `Propose(node, id)` inserts `id` into the global proposal set and processes
   it as a self-vote.
2. `processVote` intersects memberships, unions proposals, and records the
   current local vote set in `votes`.
3. For each proposal id in the merged proposal set, the receiver counts itself
   as supporting that id. For proposal ids present in the incoming message, the
   sender is counted too.
4. On a membership reduction, every `proposalVotes` entry is intersected with the
   reduced node set.
5. The first observed vote always causes a rebroadcast so the node's current
   state becomes visible to peers.
6. Once `votes == nodes`, the node checks `mayCommit`:
   every id in `proposals` must have `2 * |votesFor(id)| > |nodes|`.
7. If `mayCommit` holds, the node commits and broadcasts a `Commit` that carries
   the exact committed set.
8. A receiver accepts `Commit(commit)` only if its current local `proposals`
   already equal `commit`.

## Disconnect Handling

`Disconnect` removes the failed node from `alive`, purges queued messages, and
reprocesses survivor state against the smaller node set. Unlike older drafts,
the current model also intersects `proposalVotes` with the reduced membership.

## Safety and Liveness

Safety still requires all live committed nodes to agree on the committed proposal set.

The current model also checks liveness in a separate liveness model. Safety
keeps the full disconnect space. Liveness reuses the same transition logic, but
limits disconnects so that a majority of the original nodes remains alive.
Under weak fairness of `ProposeAny` and `DeliverAnyVote`, some node must
eventually commit a non-empty proposal set.

Recent `build/rel` runs:

- executable safety model: about 7.7 seconds, `318130` states, `1130173` transitions
- executable liveness model: about 7.8 seconds, `247165` states, `656329` transitions
- TLC safety: about 11.7 seconds, `558142` generated, `227236` distinct
- TLC liveness: about 69.5 seconds, `224500` generated, `156271` distinct

## Important Nuance

The current model uses commit payload propagation. That is a deliberate choice
in this repository's model because a payload-free commit let receivers finalize
their own local `proposals`, which was too weak for agreement.
