# Partial-Order Reduction Plan

## Goal

Reduce redundant interleavings in message-passing models when node actions are
independent, without relying on symmetry between nodes.

This targets the current engine shape in `src/engine.cpp`, which:

- enumerates all successors of `next()`
- interns states only after a full successor has been generated
- does not track which subaction produced a transition

POR should cut scheduling noise before it becomes extra transitions.

## What POR Can Improve

POR removes permutations of independent steps, not genuine state combinations.

For message-passing systems, the upside is best when:

- each step touches one node's local state
- communication is through an unordered message pool or monotone message set
- two enabled steps do not disable each other
- properties are safety/deadlock properties

In that shape, many `A; B` / `B; A` pairs collapse to one representative
ordering. This can reduce near-factorial interleaving blow-up, but it does not
make the remaining state space polynomial in general.

## Scope

First version should support only:

- reachability
- invariant checking
- deadlock checking

Do not try to preserve:

- liveness
- weak fairness
- strong fairness

Liveness-preserving POR is substantially harder and should be a later phase.

## Required Model Shape

POR needs explicit actions, not one opaque `next()` formula.

Recommended internal direction:

```cpp
struct NamedAction {
  std::string name;
  BoundNextAction<Boolean> action;
};

virtual std::vector<NamedAction> actions();
```

`next()` can remain as the public compatibility API, but the engine should gain
an action-level path for POR.

## Dependency Model

Two actions are independent at a state if:

- both are enabled
- executing one does not disable the other
- both execution orders reach the same final state

Exact independence is expensive, so the first implementation should use a
conservative dependency test. Conservative means "dependent unless clearly
independent".

Recommended first metadata per action:

- owned process or service id
- reads message set
- appends message set
- writes only local state for one process
- touches global shared state

Safe first heuristic for message-passing systems:

- actions for different processes are independent if they only:
  - read current state
  - write that process's local state
  - add messages to a set
- any action that removes messages, rewrites shared state, or depends on global
  cardinality/quorum logic is treated as dependent

This heuristic misses some reductions, but it is safe.

## Engine Design

### 1. Expand by action

For each stored state:

- compute enabled named actions
- compute a persistent set or stubborn set of representative actions
- explore only those actions from that state

### 2. Record action labels

Edges should carry the producing action:

```cpp
struct Edge {
  const State* target;
  uint32_t actionId;
};
```

This is needed for debugging, validation, and future fairness-aware work.

### 3. Keep full-state hashing

State interning should stay value-based. POR is an exploration reduction, not a
replacement for state deduplication.

## Minimal Algorithm

Start with a small persistent-set algorithm:

1. Enumerate enabled actions at state `s`.
2. Pick one seed action `a`.
3. Build a persistent set `P`:
   - start with `a`
   - if some enabled action may disable or conflict with an action in `P`, add
     it to `P`
   - repeat to closure
4. Explore only actions in `P`.

Conservative fallback:

- if dependency is unclear, include the action
- if all actions look dependent, POR degenerates to normal exploration

This makes the first implementation safe and easy to validate.

## Expected Payoff

Expect the biggest gains when:

- many services react independently to different messages
- message send is monotone
- local handlers are mostly process-local

Expect modest gains when:

- quorum checks or global guards couple actions quickly
- receive/send choices share the same message pool heavily
- many actions are conservatively classified as dependent

Consensus-style protocols usually fall in the middle: POR helps, but it will
not eliminate the core combinatorial growth by itself.

## Validation Plan

- add small diamond tests where two actions commute and only one ordering needs
  exploration
- compare explored transition counts with POR on/off
- keep POR behind an engine flag
- verify that POR-on and POR-off agree on reachable states and invariants for
  the same model

## Recommended Phases

1. Add named action expansion beside `next()`.
2. Add conservative action metadata and dependency checks.
3. Add persistent-set exploration for safety/deadlock only.
4. Add benchmarks using message-passing samples such as Paxos-like models.
5. Revisit liveness only after the safety path is stable.
