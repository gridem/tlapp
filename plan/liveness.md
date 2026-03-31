# Liveness Plan

## Goal

Add a first liveness-checking layer to the engine that is:

- semantically separate from the existing `Boolean` assignment logic
- efficient enough for graph-based checking
- consistent with the current TLA++ DSL style
- small enough to implement incrementally

The first supported fragment should be conjunctions of:

- `WF(A)` where `A` is an action formula
- `SF(A)` where `A` is an action formula
- `<>(P)` where `P` is a state predicate

This is intentionally smaller than full temporal logic. It is enough to make progress on practical properties such as consensus progress under fairness assumptions.

## Minimal Public API

Keep the existing model API for `init()` and `next()` unchanged. Add one optional method:

```cpp
virtual std::optional<LivenessBoolean> liveness() { return {}; }
```

Introduce:

```cpp
struct LivenessBoolean {
  std::vector<Boolean> weakFairness;
  std::vector<Boolean> strongFairness;
  std::vector<Boolean> eventually;
};
```

Constructors:

```cpp
LivenessBoolean wf(Boolean action);
LivenessBoolean sf(Boolean action);
LivenessBoolean eventually(Boolean state);
LivenessBoolean operator&&(LivenessBoolean, LivenessBoolean);
```

Usage:

```cpp
std::optional<LivenessBoolean> liveness() override {
  return wf(phase1b()) && wf(phase2b()) && eventually(chosen());
}
```

Notes:

- `wf(Boolean)` and `sf(Boolean)` treat the `Boolean` as an action formula.
- `eventually(Boolean)` treats the `Boolean` as a state predicate.
- `&&` means conjunction of liveness obligations by merging the corresponding vectors.
- No vector-based API is exposed to the user directly.

## Semantics

### Boolean meaning

The existing `Boolean` DSL remains unchanged:

- `init()` and `next()` still use the current assignment-aware semantics.
- liveness checks always evaluate formulas in check mode.
- `=` remains C++ assignment.
- `==` remains the DSL relation operator, consistent with the README.

### Liveness meaning

The `LivenessBoolean` value represents conjunction of all stored obligations:

- every entry in `weakFairness` means one `WF(A)`
- every entry in `strongFairness` means one `SF(A)`
- every entry in `eventually` means one `<>(P)`

Important:

- `wf(a) && wf(b)` means `WF(a) /\ WF(b)`
- `wf(a || b)` means `WF(a \/ b)`
- these are not equivalent in general, so they must remain distinguishable

## Engine Design

The current engine is sufficient for reachability and invariant checks, but not for liveness, because it only stores:

- unique states
- one predecessor per state
- no full transition graph
- no action labels on edges

For liveness, the engine must build and retain the full reachable graph.

### New internal data

Introduce stable node ids:

```cpp
using NodeId = uint32_t;
```

Store:

- canonical state storage
- adjacency list for all transitions
- reverse adjacency if useful for counterexample reconstruction
- per-node cached results for state predicates
- per-node cached enabledness for action formulas
- per-edge cached action satisfaction

Suggested shape:

```cpp
struct Edge {
  NodeId to;
  std::vector<uint8_t> weakHits;
  std::vector<uint8_t> strongHits;
};

struct Node {
  State state;
  std::vector<Edge> out;
  std::vector<uint8_t> eventuallyHolds;
  std::vector<uint8_t> weakEnabled;
  std::vector<uint8_t> strongEnabled;
};
```

This can later be optimized to bitsets if the number of obligations is bounded or known to be small.

### Exploration changes

Refactor exploration so that:

1. each source state is processed exactly once for expansion
2. every resulting transition is recorded, even if the target state already exists
3. the engine separates:
   - state interning
   - edge recording
   - queueing newly discovered nodes

The existing predecessor-only map should remain only for simple reachability traces, or be replaced by a richer counterexample structure.

## Checking Algorithm

### Phase 1

Run ordinary reachability as today, but build the full finite transition graph.

### Phase 2

Evaluate liveness obligations on the graph:

- `eventually[i]` on nodes
- enabledness of `weakFairness[i]` and `strongFairness[i]` on nodes
- action occurrence of each fairness formula on edges

All of these checks must run in check mode.

### Phase 3

Search for reachable SCCs that violate the liveness obligations.

An SCC is a liveness counterexample if:

- for some `<>(P)`, every node in the SCC has `!P`
- for some `WF(A)`, `A` is enabled in every node of the SCC and no edge in the SCC satisfies `A`
- for some `SF(A)`, `A` is enabled in at least one node of the SCC and no edge in the SCC satisfies `A`

If such an SCC exists, produce a liveness failure.

### Counterexample form

Liveness failures need a lasso, not just a prefix:

- finite prefix from an initial state to the SCC
- cycle inside the SCC

This is different from current `stop()` / `ensure()` tracing.

## Stuttering and Deadlocks

The engine should define how deadlocks behave for liveness.

Recommended first step:

- if a node has no outgoing edges, treat it as a stuttering self-loop for liveness checks

Reason:

- this better matches temporal reasoning over infinite behaviors
- otherwise `<>(P)` can be misclassified on terminal `!P` states

This should be documented explicitly because it affects the meaning of liveness results.

## Incremental Implementation Plan

### Step 1

Add `LivenessBoolean` and `IModel::liveness()`.

### Step 2

Refactor engine graph storage:

- keep full adjacency
- keep node ids
- preserve reachability behavior

### Step 3

Implement `<>(P)` checking only.

This gives the simplest useful liveness check and validates:

- graph construction
- SCC detection
- lasso counterexamples

### Step 4

Add `WF(A)` support.

This requires:

- enabledness checks on nodes
- edge checks for action occurrence

### Step 5

Add `SF(A)` support.

### Step 6

Optimize storage and checking:

- replace `std::vector<uint8_t>` with bitsets if needed
- deduplicate predicate evaluation
- reduce repeated context setup

## Test Plan

Add tests in `tests/engine.cpp` or a new `tests/liveness.cpp`.

### `<>(P)` success

Model with a finite graph where all cycles eventually reach `P`.

Expected:

- liveness passes

### `<>(P)` failure

Model with a reachable cycle where `P` is always false.

Expected:

- liveness fails
- counterexample includes prefix and loop

### `WF(A)` distinction

Model where `A || B` keeps happening forever but `B` never happens even though it is continuously enabled.

Expected:

- `wf(A || B)` passes
- `wf(A) && wf(B)` fails

This test is important because it validates the semantics that motivated the internal vector-of-obligations design.

### `SF(A)` distinction

Model where `A` becomes enabled infinitely often but not continuously, and never occurs.

Expected:

- `WF(A)` may pass
- `SF(A)` fails

### Deadlock / stuttering behavior

Model that reaches a deadlock state with `!P`.

Expected:

- `<>(P)` fails if deadlock is treated as infinite stuttering

## Performance Notes

The implementation should avoid path-based temporal evaluation.

Effective liveness checking here means:

- build graph once
- cache predicate checks on nodes and edges
- run SCC-based analysis

This is the main reason for introducing `LivenessBoolean` as a compiled set of obligations instead of trying to reuse ordinary `Boolean` evaluation directly.

## Complexity Considerations

Use the following notation:

- `V` = number of reachable states
- `E` = number of reachable transitions
- `W` = number of weak fairness obligations
- `S` = number of strong fairness obligations
- `P` = number of eventuality obligations

### Reachability graph construction

If state interning uses hashing with expected `O(1)` lookup/insert, graph construction is:

- expected time: `O(E)` plus the cost of evaluating `next()`
- memory: `O(V + E)` plus the size of stored states

More explicitly, the engine cost is:

- one canonical state object per reachable state
- one stored edge per reachable transition
- one queue push per newly discovered state

Compared to the current engine, this keeps the same asymptotic reachability cost, but memory increases because all transitions are retained instead of only one predecessor per state.

### Liveness predicate caching

If all obligations are evaluated separately and cached:

- eventuality checks on nodes: `O(P * V)`
- fairness enabledness checks on nodes: `O((W + S) * V)`
- fairness action-hit checks on edges: `O((W + S) * E)`

This assumes the cost of one predicate evaluation is treated as constant at the graph-algorithm level.

In practice, the real cost is:

- `O(P * V * C_state)`
- `O((W + S) * V * C_enabled)`
- `O((W + S) * E * C_edge)`

where:

- `C_state` is the cost of evaluating one state predicate
- `C_enabled` is the cost of checking whether an action formula is enabled in a state
- `C_edge` is the cost of checking whether an edge satisfies an action formula

### SCC search

If Tarjan or Kosaraju is used:

- SCC decomposition time: `O(V + E)`
- SCC decomposition memory: `O(V)`

This part is linear and should not be the bottleneck.

### Checking obligations inside SCCs

Naive checking would scan every SCC for every obligation:

- time: `O((V + E) * (W + S + P))`

This is still acceptable for small obligation counts, but it does unnecessary repeated work.

A better implementation is to summarize each SCC with cached bitsets:

- `allNodesNotEventually[i]`
- `allNodesWeakEnabled[i]`
- `anyNodeStrongEnabled[i]`
- `anyEdgeWeakTaken[i]`
- `anyEdgeStrongTaken[i]`

Then SCC validation becomes linear in SCC size with compact bit operations, keeping the total check close to:

- time: `O(V + E + summary_cost)`

where `summary_cost` is proportional to the number of bitset words touched while aggregating SCC data.

### Counterexample reconstruction

For a failing SCC:

- prefix reconstruction to the SCC: `O(V + E)` in the worst case
- cycle extraction inside the SCC: `O(size_of_SCC)`

This is not on the hot path unless a failure is found.

## Expected Bottlenecks

The main cost is not SCC decomposition. The main cost is evaluating generic `Boolean` action formulas repeatedly.

This is especially important for fairness:

- `WF(A)` needs to know if `A` is enabled on a node
- `WF(A)` / `SF(A)` also need to know if an edge corresponds to an `A` step

If implemented naively, this can require re-running each fairness action formula many times, which may dominate total runtime.

### Naive fairness cost

If every fairness formula is re-evaluated independently on every node and edge, the effective cost can approach:

- `sum over states of all fairness-action evaluations`
- plus `sum over edges of all action-hit checks`

This can be much larger than the base reachability cost if the fairness formulas are complex.

### Efficient fairness strategy

The preferred direction is:

1. evaluate fairness formulas in check mode only
2. cache enabledness per node once
3. cache action-hit per edge once
4. aggregate obligation results with bitsets during SCC processing

This keeps the graph algorithm linear after the predicate cache is built.

## Memory Tradeoffs

The main memory consumers are:

- stored `State` values
- adjacency lists
- cached per-node obligation results
- cached per-edge obligation results

With byte vectors:

- node cache cost is roughly `V * (P + W + S)` bytes
- edge cache cost is roughly `E * (W + S)` bytes

With bitsets:

- node cache cost becomes roughly `V * ceil((P + W + S) / 8)` bytes
- edge cache cost becomes roughly `E * ceil((W + S) / 8)` bytes

So bitsets are likely worth adding once the design is stable, especially when:

- `E` is large
- fairness obligations are more than just a few entries

## Recommended Efficiency Milestones

### First implementation

Accept:

- simple vectors of `uint8_t`
- separate evaluation of liveness obligations
- one SCC pass

This keeps the implementation understandable and correct.

### First optimization pass

Add:

- compact bitset storage
- SCC summaries using bitwise aggregation
- explicit node ids instead of pointer-heavy maps where practical

### Long-term optimization

If liveness checking becomes a significant feature, consider:

- compiling obligations into indexed internal clauses
- sharing results between `next()` expansion and fairness-action checks
- reducing repeated evaluation of identical subexpressions
- labeling generated transitions during exploration when possible

The last item is particularly important because it can reduce the need to re-check whether an edge satisfies a fairness action formula.

## Open Questions

These can be deferred until after the first implementation:

- whether to expose separate `weakFairness()` / `strongFairness()` methods instead of `liveness()`
- whether to represent cached checks as dynamic bitsets instead of byte vectors
- whether to support richer temporal formulas later
- whether to expose liveness failures as dedicated exception types

## Non-Goals for Phase 1

Do not implement yet:

- arbitrary nesting of temporal operators
- implication and full temporal normalization
- fairness reduction rules such as automatically collapsing `WF(A) /\ WF(B)`
- symbolic or partial-order liveness reduction

The first milestone should be a correct finite-graph checker for conjunctions of `WF`, `SF`, and `<>`.
