# Leaderless Consensus Family Verification

This article explains the leaderless consensus models as one algorithm family. It presents the progression from an intentionally naive protocol to stronger set- and prefix-based designs, with emphasis on the problem each variant addresses, the object it commits, and the safety and liveness properties established by the executable TLA++ models.

This work is a continuation of the earlier article [Replicated object, part 7: masterless](https://gridem.blogspot.com/2016/05/replicated-object-part-7-masterless.html). The difference is that the earlier text described the ideas, while this effort carries them into checked finite models with explicit safety proofs and separate liveness checks.

The implementation is available at [github.com/gridem/tlapp](https://github.com/gridem/tlapp), and the detailed variant notes live at [github.com/gridem/tlapp/tree/main/docs/leaderless-consensus](https://github.com/gridem/tlapp/tree/main/docs/leaderless-consensus).

<!--more-->

## Problem

The common problem is leaderless replication of client proposals. There is no master responsible for ordering updates. Any node may introduce a proposal locally, and the cluster must converge through peer-to-peer communication.

The current models are intentionally small:

- `Nodes = {0, 1, 2}`
- `MessageIds = {10, 11, 12}`

This is not a simplification for convenience only. The small domain makes full state exploration practical and keeps the verification focused on protocol semantics instead of payload volume.

The variants split into two families:

- `Sore`, `Calm`, `Flat`, and `Most` try to agree on a committed proposal set.
- `Rush` tries to agree on a committed sequence prefix.

## Common Model Frame

The executable TLA++ models explore all enabled transitions nondeterministically. They are not simulating one chosen fair execution. They are enumerating all reachable combinations of local state, message queues, and enabled actions.

In practice that means the following actions are explored whenever enabled:

- `Propose`
- message delivery
- `Disconnect` for the variants that include failures

The explored state has two layers:

- engine-level global state, which includes the in-flight message queues, the current live/disconnected set where applicable, and the global proposed set
- per-node local state, which includes each node's protocol-specific fields

So queue contents are part of the global engine state, not part of any one node's local state. Two executions with identical per-node contents but different in-flight messages are different reachable states.

Every model also has a global `proposed` set. That is the set of proposal ids that have already entered the system. In the current finite models:

- `Sore`, `Calm`, `Most`, and `Rush` allow a pristine node to propose any not-yet-proposed id.
- `Flat` reduces proposal generation to one fixed proposal per node: `0 -> 10`, `1 -> 11`, `2 -> 12`.

The set-based variants use majority logic over a current node view or live set. With three nodes, quorum is two. That is why majority-preserving liveness models allow at most one disconnect.

The TLA++ work also separates safety and liveness where that improves clarity. Safety and liveness do not ask the same question, so they should not always use the same environment assumptions.

## Algorithm Family

The five variants form a progression from a naive baseline to an ordered prefix protocol.

### Sore

`Sore` is the intentionally naive failure case. Each node tracks which peers it has heard from, which proposals it knows, and whether it is already committed. The rule is simple: accumulate votes and commit once the local condition says so.

This simplicity is exactly why it is useful as a learning example. `Sore` finds a counterexample and demonstrates why stronger convergence rules are needed. It is not a protocol that should be treated as a usable starting point for production design. It is illustrative only. The production-oriented variants in this family are the ones that hold both safety and liveness in the checked models.

One concrete counterexample is short:

1. `Propose(0, 10)`
2. `Propose(1, 11)`
3. `DeliverVote(0 -> 2)`
4. `DeliverVote(1 -> 2)`
5. `Disconnect(1)`
6. `DeliverVote(2 -> 0)`

After step 4, node `2` has already committed `{10, 11}`. After step 5, node `0` shrinks its current membership view to `{0, 2}`. After step 6, node `0` has votes from every node in that reduced view and commits only `{10}`.

That is the whole bug in one trace: `Sore` commits when every node in the current view has voted, but it does not require those votes to certify the same proposal set. A membership change can therefore make a weaker local commit condition look complete even though another live completed node has already committed a larger set.

### Calm

`Calm` introduces a stronger notion of stability. A node distinguishes between:

- still voting
- eligible to commit
- no longer eligible because conflicting evidence arrived

The core rule is that a node must complete a full pass over its current membership view while its proposal set stays stable in the commit-eligible phase. If proposals change or the membership view changes, that pass is no longer valid and the node must start another one.

This is the first variant that holds both safety and liveness in the current finite model.

### Flat

`Flat` preserves more information in flight. Each vote message carries:

- `proposals`
- `nodes`
- `votes`

When a node processes a vote, it merges the incoming and local proposal sets, node views, and vote support. That makes the protocol more merge-driven and less phase-driven than `Calm`.

The practical consequence is that `Flat` is much heavier to explore. It preserves richer summaries, produces more distinct in-flight message states, and has a larger reachable graph. The current reduced model still holds, but it is materially more expensive than `Calm`.

### Most

`Most` moves from whole-pass stability to majority support tracked per proposal. In addition to the aggregate local vote set, a node tracks for each proposal id which nodes currently count as supporting that proposal.

Commit requires two things:

- the local vote set must cover the current local membership view
- every locally known proposal must have majority support

The important nuance is that per-proposal support is independent data. It cannot be derived safely from the aggregate vote set alone.

One important modeling correction in the final TLA++ model was to make commit messages carry the explicit committed proposal set. Without that payload, a receiver could finalize its own current proposal set instead of the sender's committed one.

### Rush

`Rush` is the most advanced variant. It does not commit an unordered set. It commits a sequence and requires committed sequences to remain prefix-compatible.

Each node tracks:

- an unordered set of known proposal ids
- one ordered message sequence per node, each with a generation
- promise votes for candidate prefixes
- its own committed prefix

The algorithm grows commitment one position at a time. If a quorum of node sequences has the same id at the next uncommitted position, the candidate prefix can be extended. Support for that prefix is recomputed from current node sequences, promise votes are intersected with that support, and a quorum condition decides whether the prefix is committed.

If no majority exists at the current position, the node sorts its own remaining suffix once. That canonicalizes the local tail and can unlock later majority checks.

The current reduced `Rush` model omits disconnect. That is intentional. The ordering logic is already the most expensive and subtle part of the family, and the current design does not rely on timeout-driven failure handling to make progress. That makes it the most robust variant in this set from a protocol structure perspective: progress is not gated on waiting for a timeout before moving forward. In practical terms, that removes one common source of tail-latency inflation. The checked models support that structural claim, but they do not prove an absolute p99 latency bound.

## Message Semantics

The variants can also be understood by the amount of meaning they place into messages.

- `Sore` and `Calm` mostly exchange known proposals and the current node view.
- `Flat` adds explicit vote support into vote messages.
- `Calm` and `Most` use commit messages that carry the committed proposal set explicitly.
- `Rush` gossips a full core state in each state message.

This matters directly for state-space size. Richer messages make local reconstruction easier, but they also create more distinct in-flight states for the checker.

## Safety and Liveness

The set-based variants check agreement on committed proposal sets. `Rush` checks prefix compatibility instead of equality, because a later committed sequence may extend an earlier one without violating correctness.

> For `Rush`, equality would be the wrong invariant. The correct safety
> property is that committed prefixes do not branch.

The liveness checks are intentionally positive. The current question is not "does the system become idle?" but "does commit happen?"

More concretely:

- `Calm`, `Flat`, and `Most` require that some node eventually commits a non-empty proposal set.
- `Rush` requires that some node eventually commits a non-empty prefix.

Fairness is action-level rather than a coarse `WF(Next)` rule. That gives a more meaningful progress assumption because it talks directly about proposal and delivery actions.

## Verification Results

The current results are structurally clean:

- `Sore` fails, as expected.
- `Calm`, `Flat`, and `Most` hold in the current finite models.
- `Rush` is the most advanced checked variant, using timeout-free prefix-based live consensus.

That statement should be read precisely. The result is not an informal argument that these variants "seem right." The result is that, within the current finite abstractions, the safety invariants were model-checked and the liveness properties were checked separately under explicit fairness assumptions.

This article focuses on the executable TLA++ side. That is the main engineering loop for this work: it is fast enough to refine the algorithms, rerun the full finite checks, and validate design corrections such as disconnect behavior, commit payload propagation, and `Rush` generation and promise handling.

## Performance Lessons

Two practical engineering lessons came out of this work.

First, shared data-structure changes matter. Moving from tree-based containers to shared flat and inplace containers improved the leaderless-consensus models while keeping the algorithms readable. That is the right optimization boundary.

Second, not every low-level optimization is worth keeping. An inline-storage replacement for `Value` was benchmarked and then removed. The gain over the simpler `unique_ptr` design was small, while the complexity cost was real.

> A negative optimization experiment is still useful if it sharpens the design
> boundary.

## Reading Order

A practical way to study the family is:

1. start from `Sore` as the naive baseline
2. then move through `Calm`, `Flat`, and `Most` as the set-based progression
3. and finish with `Rush` as the ordered prefix model

After that, the source files and the detailed GitHub notes read much more naturally because the design ladder is already clear.

## Visual Walkthroughs

The HTML article includes two inline interactive sections so the blog-post version can stand on its own as a single file.

### Set-Based Trace Walkthrough

The set-based walkthrough covers:

- `Sore`, with the counterexample trace and an explicit red-marked safety violation at the final step
- `Calm`, with a stable-pass convergence trace on `{10,11}`
- `Flat`, with merge-heavy vote propagation and final commit convergence
- `Most`, with per-proposal support tracking and final commit convergence

The structure is intentionally compact:

- current step at the top
- three aligned node cards
- vote and commit queues in the same panel

That makes the final state easy to read. For the holding variants, the trace ends with all nodes committed to the same set. For `Sore`, it ends at the first violating state where live completed nodes disagree.

### Rush Prefix Walkthrough

The `Rush` walkthrough is separate because the state shape is different. It shows:

- one ordered sequence per node
- the current candidate prefix
- the committed prefix
- whether each node is currently in `support(prefix)`
- whether each node is currently in `promise.votes(prefix)`
- active promises rendered inside each node card

This makes the core `Rush` distinction visible: correctness is about prefix-compatibility and prefix extension, not equality of full local sequences.

## Conclusions

This work now contains a compact but meaningful ladder of leaderless consensus models:

- one expected failure: `Sore`
- three set-agreement variants that currently hold: `Calm`, `Flat`, `Most`
- one most-advanced prefix-commit variant: `Rush`, using timeout-free prefix-based live consensus

The result is useful both algorithmically and operationally. The protocol family is explicit, the verification assumptions are documented, and the performance behavior of the executable TLA++ models is measured rather than guessed.

More importantly, this is not just a rewrite of an older article. It is a continuation of that effort with a much stronger standard of evidence: finite-model safety proofs, explicit liveness checks, and repeated alignment between the executable and declarative specifications.
