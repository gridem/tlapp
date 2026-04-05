# Boolean Optimization Plan

## Goal

Improve the implementation behind `Boolean` so it is easier to reason about,
more efficient, and less dependent on runtime tricks such as nested
`std::function`, lazy lambda indirection, and phase-sensitive `Context`
mutation.

The intent is to keep the external DSL stable:

- `x == 1`
- `x++ == 2`
- `a && b`
- `a || b`
- quantifiers and existing samples/tests

The main work is internal: simplify representation, separate concerns, and
reduce runtime overhead.

Priority order for decisions in this plan:

1. code clarity and semantic explicitness
2. preserving the current DSL surface
3. reducing unnecessary runtime dispatch and allocation
4. micro-optimizing only after the architecture is simpler

## Current Problems

### 1. `Boolean` mixes different concepts

`Boolean` currently represents both:

- a plain predicate that should evaluate to `true` or `false`
- a symbolic transition result that may produce multiple assignment branches

That makes the type hard to use correctly. Examples:

- `!` on `LogicResult` throws at runtime
- `skip`, `ensure`, and `stop` accept `Boolean`, but the engine later rejects
  non-plain boolean results

This is a type-model problem, not just an implementation detail.

### 2. The execution model is closure-heavy

Assignments are represented as deferred callables:

- `AssignsResult` is a `std::function<bool(Context&)>`
- `LogicResult` is a vector of those callables
- `&&` combines branches by creating new lambdas that call previous lambdas

This creates several costs:

- repeated type erasure
- heap allocation risk from `std::function`
- poor debuggability
- hard-to-predict copying/moving behavior
- no clear data model for a branch

### 3. `Context` has too many responsibilities

`Context` is used as:

- variable registry
- runtime storage for current state
- runtime storage for next state
- phase tracker (`Init` / `Next`)
- check-mode switch
- validation gate for allowed registration

This makes evaluation order matter in places where it should not, especially
during init and branch handling.

### 4. Lazy evaluation is implemented indirectly

`evaluate_lazy` wraps extracted operands into tagged lambdas. This works, but it
adds another abstraction layer on top of already-erased expressions. The
current boolean operators are then forced to re-dispatch on a mix of:

- `bool`
- `LogicResult`
- `BooleanResult`
- lazy callables

The logic is clever, but too hard to maintain.

### 5. Branch normalization eagerly materializes combinations

`&&` currently expands branch combinations eagerly. That is semantically valid,
but potentially expensive for large disjunctions, quantifiers, or larger models.

### 6. Quantifiers do partial predicate generation, but still pay too much at runtime

`forall` and `exists` already invoke the user predicate once to build
`predicateExpr`, then reuse it across element iteration. That is the right high
level direction, but the current execution path still has avoidable cost:

- quantified sets are copied in the runtime loop instead of being borrowed
- quantified element rebinding relies on a heap-allocated shared pointer slot
- each element goes through the generic boolean combiner path, even for pure
  predicate quantifiers
- `filter` duplicates the same overall pattern and should share the same kernel

This area deserves dedicated optimization work instead of being treated as a
small follow-up to generic boolean cleanup.

## Design Principles

### Keep the DSL stable

The user-facing syntax should remain the same unless there is a very strong
semantic reason to change it.

### Make branch structure explicit

A branch should be represented as data, not as a nested closure.

### Move errors to the type system where possible

If an expression is only valid as a pure predicate, the API should say so.

### Separate read-time and write-time concerns

Reading current state, building next-state assignments, and registering schema
should not all happen through the same mutable object.

### Optimize after representation is fixed

The biggest gains will come from better internal structure, not from local
micro-optimizations inside the current closure-based code.

### Prefer simple internal APIs over macro-heavy internals

User-facing DSL helpers such as `$A`, `$E`, and `$if` are worth preserving.
Internal implementation, however, should prefer ordinary named types and
functions over increasingly generic macro layers.

If new helpers are needed, prefer semantic helpers such as:

- `bind(expr, PredicateMode{})`
- `bind(expr, InitMode{})`
- `bind(expr, NextMode{})`

over more generic forwarding/metaprogramming macros.

### Reuse `evaluate.h` as a small shared substrate

`src/evaluate.h` is already the closest thing to a common expression execution
layer in the codebase. It should be reused more aggressively where it improves
clarity, but with one constraint:

- it should stay focused on generic expression preparation, extraction, binding,
  and finalization
- it should not become the place where boolean role semantics are hidden behind
  increasingly generic helper macros

Recommended use of `evaluate.h`:

- shared extraction of immediate vs expression operands
- common bind/evaluate plumbing for generic operators
- small reusable helpers for predicate/init/next binding

Not recommended:

- pushing all boolean and quantifier semantics into one generic `evaluate_*`
  layer that becomes harder to reason about than the code it replaced

## Architecture Options

### Option A: Keep one generic runtime evaluator and clean it up

This means preserving the current model where one expression is interpreted
differently based on runtime context and flags, but making the code somewhat
cleaner.

Pros:

- smallest refactor
- lowest compile-time cost
- lowest short-term risk

Cons:

- keeps semantic mode dispatch in the hot path
- keeps `Context` overloaded
- only limited clarity improvement
- likely leaves quantifiers and boolean operators harder to reason about than
  necessary

### Option B: Fully specialize all expression nodes for every mode

This means generating separate implementations for all relevant operations in:

- `PredicateMode`
- `InitMode`
- `NextMode`

Pros:

- best theoretical runtime performance
- very little semantic branching in evaluation hot paths

Cons:

- highest implementation complexity
- biggest compile-time and code-size cost
- hardest design to maintain and evolve
- likely overkill relative to the project’s stated priorities

### Option C: Recommended hybrid

Keep expression construction generic, then bind the expression once into a
role-specific evaluator:

- `BoundPredicate` for pure check usage
- `BoundInitAction` for init-state construction
- `BoundNextAction` for next-state construction

Use runtime normalized data only where the structure is genuinely dynamic:

- disjunction/conjunction branch expansion
- quantifier expansion over runtime containers
- engine storage of normalized transition alternatives

Pros:

- best balance of clarity, performance, and implementation cost
- removes repeated semantic dispatch from hot evaluation paths
- keeps compile-time specialization where it is valuable
- avoids forcing everything into templates even when the engine needs runtime
  structure anyway
- preserves the external DSL

Cons:

- still requires a runtime IR for normalized transition data
- requires careful boundary design between binding-time specialization and
  runtime branch materialization

## Recommendation

Choose Option C.

It matches the project priorities better than either extreme:

- clearer than the current runtime-dispatch approach
- much less complex than fully specializing all internals
- fast enough to remove the most wasteful runtime checks
- compatible with preserving the existing DSL and macro surface

Important guidance:

- specialize semantic nodes such as `==`, quantifiers, and boolean combiners
- keep arithmetic and plain pure comparisons generic unless benchmarks show a
  need
- reduce internal macro cleverness as part of the refactor rather than adding
  more of it
- use `src/evaluate.h` as the common low-level substrate where it removes
  boilerplate, but keep semantic decisions in the boolean/quantifier binding
  layer

## Proposed End State

Internally, keep one generic DSL expression form during construction, then bind
it into role-specific evaluators and use explicit branch data only after runtime
normalization is actually needed.

Possible shape:

```cpp
auto expr = makeFormula(...);

BoundPredicate pred = bind(expr, PredicateMode{});
BoundInitAction init = bind(expr, InitMode{});
BoundNextAction next = bind(expr, NextMode{});
```

Those bound forms can then evaluate into simpler result shapes:

- `BoundPredicate -> bool`
- `BoundInitAction -> TransitionResult`
- `BoundNextAction -> TransitionResult`

One possible runtime normalized shape for transition results:

```cpp
struct Guard {
  /* pure condition evaluated against an immutable view of current/known state */
};
struct Assignment { /* target variable + expression/value for assignment */ };

using BranchOp = std::variant<Guard, Assignment>;

struct Branch {
  std::vector<BranchOp> ops;
};

struct Logic {
  std::vector<Branch> branches;
};

using TransitionResult = std::variant<bool, Logic>;
```

This is only a sketch, not a final API. The important points are:

- the DSL expression stays generic while it is being built
- semantic meaning is fixed when the expression is bound to a role
- the engine receives explicit transition data instead of opaque lambdas

Here "immutable view of the current state" means:

- reads current variables and, where needed, next variables already assigned in
  the branch so far
- does not register variables
- does not mutate engine phase flags
- does not write assignments as a side effect of evaluation

Terminology note:

- `Guard` is intentionally close to "state predicate" / "action condition"
- `Assignment` is the side-effectful part of a transition branch
- `Branch` is one conjunctive alternative inside a larger boolean formula
- `Logic` is the disjunction of those branches

Representation note:

- `BranchOp` here is a sketch of the runtime normalized form, not a statement
  that the whole implementation should become runtime-typed
- compile-time expression types are still useful while building and simplifying
  the DSL expression tree
- one promising direction is to bind the same DSL expression into several
  specialized evaluator modes, for example:
  - predicate / check mode
  - init-transition mode
  - next-transition mode
- a runtime representation is still needed once branches are normalized and
  handed to the engine, because branch count, branch length, and quantifier
  expansion are often known only at runtime
- `std::variant<Guard, Assignment>` is one practical choice for that runtime IR,
  not the only acceptable one
- if later refactoring proves that a more structured representation preserves the
  same order semantics more efficiently, that should be preferred

## Implementation Plan

### Phase 0: Freeze Current Semantics

Before changing internals, expand tests around the current behavior.

Add tests for:

- init registration rules
- partial init failures
- next-state assignment failures
- `&&` and `||` normalization with mixed comparisons and assignments
- quantifier behavior with branch-producing predicates
- rejection of branch-producing expressions in `skip`, `ensure`, and `stop`

Deliverables:

- broader coverage in `tests/boolean.cpp`
- broader coverage in `tests/engine.cpp`
- a few focused tests around quantifier/branch interaction

Success criteria:

- current semantics are documented by tests before refactoring begins

### Phase 1: Clarify the Type Model

Introduce separate internal concepts for:

- generic unbound DSL expressions
- pure predicate evaluators
- init-transition evaluators
- next-transition evaluators
- runtime normalized transition results

Options:

- keep `Boolean` as the public name, but use clearer internal binding/result
  types
- or add separate public/internal aliases such as `FormulaExpr`,
  `BoundPredicate`, and `TransitionResult`

Recommended direction:

- preserve current public signatures for `init()` and `next()`
- require pure predicate semantics for `skip()`, `ensure()`, and `stop()`
- move invalid combinations toward compile-time rejection where feasible
- make binding explicit: the same expression tree should be bindable into
  `PredicateMode`, `InitMode`, or `NextMode`
- prefer mode-specialized lowering/binding of the expression tree rather than
  one generic evaluator that keeps checking execution mode at runtime

Suggested internal evaluator modes:

- `PredicateMode`: pure read-only checks for `skip`, `ensure`, `stop`, and other
  check-only usage
- `InitMode`: init-state transition construction
- `NextMode`: next-state transition construction

This is close to the idea of generating multiple implementations for one DSL
expression and selecting the appropriate one when the expression is bound into a
specific engine role.

Possible implementation approach:

- keep `prepare`, `extract`, and `finalize`-style utilities in `src/evaluate.h`
- add a small binding layer there or adjacent to it for
  `bind(expr, PredicateMode{})`, `bind(expr, InitMode{})`, and
  `bind(expr, NextMode{})`
- keep `==`, `&&`, `||`, quantifiers, and related semantic lowering logic out of
  the generic `evaluate_*` helpers

Deliverables:

- clearer naming for unbound expressions, bound evaluators, and runtime results
- less runtime rejection for obviously invalid usages
- less runtime semantic branching in hot evaluation paths

### Phase 2: Replace `AssignsResult` with Explicit Branch Data

This is the main runtime representation refactor.

Replace:

- `AssignsResult : std::function<bool(Context&)>`
- `LogicResult : std::vector<AssignsResult>`

With explicit data describing:

- checks that must hold
- assignments that must be applied

Why this matters:

- branch composition becomes data concatenation instead of lambda nesting
- `&&` and `||` become easier to inspect and optimize
- debugging becomes much easier
- branch deduplication becomes possible later
- the engine gets a stable runtime IR while binding remains mode-specialized

Migration strategy:

1. Introduce explicit bound evaluator types for predicate/init/next roles.
2. Introduce new internal branch/result types alongside the existing ones.
3. Teach the engine to consume the new representation.
4. Port `==`, `&&`, `||`, and quantifiers to produce it through bound
   evaluators.
5. Remove old closure-based branch types after tests are green.

Deliverables:

- no `std::function<bool(Context&)>` in boolean branch representation
- simpler `and`/`or` implementation
- clearer split between binding-time specialization and runtime branch storage

### Phase 3: Split `Context` Responsibilities

Refactor `Context` into more focused roles.

Suggested separation:

- schema/registration state
- read-only evaluation view of current state
- write-side builder for next/init assignments
- execution flags for engine-only flow control

Minimum acceptable outcome:

- registration logic no longer depends on branch execution order
- evaluation does not mutate registration state implicitly
- predicate evaluation no longer needs to infer semantic role from mutable
  context flags

Benefits:

- easier reasoning about init semantics
- fewer phase-sensitive hidden side effects
- easier future optimization and caching

Deliverables:

- reduced `Context` API surface
- less logic in `ensureRegistered`
- clearer engine control flow

### Phase 4: Simplify Boolean Operator Evaluation

After branch data is explicit, simplify boolean operator internals.

Refactor goals:

- remove `resultOf`/`binaryBooleanOp` recursion-heavy dispatch
- reduce reliance on lazy tagged lambdas for boolean operations
- keep short-circuit semantics where they are logically valid
- align operator code with bound evaluator modes instead of implicit context
  interpretation

Likely direction:

- evaluate operands into a normalized result type early
- combine normalized results with straightforward helper functions
- where beneficial, instantiate/bind specialized operator implementations per
  evaluator mode so checks like `ctx.isCheck()` and init-vs-next semantic
  dispatch are not paid repeatedly during runtime evaluation
- keep arithmetic and other purely functional operators generic unless
  profiling shows real benefit from further specialization

Deliverables:

- shorter and more direct boolean operator code
- easier-to-read rules for `bool` vs branch combinations

### Phase 5: Specialize Quantifier Evaluation

Quantifiers should get their own execution strategy instead of being a thin
wrapper around the generic boolean combiner.

Main goals:

- keep construction-time predicate generation, but compile it into a more direct
  evaluator shape
- avoid copying the quantified set on each evaluation
- provide a fast path for pure boolean predicates
- share the core machinery between `forall`, `exists`, and `filter`
- optionally unroll small immediate sets at construction time
- keep user-facing quantifier macros stable while simplifying their internal
  implementation

Recommended direction:

1. Fix the obvious container-copy issue in quantifier/filter loops by borrowing
   the extracted set instead of materializing `auto set = ...`.
2. Generate a predicate kernel once, conceptually closer to
   `(Context&, const Elem&) -> Result`, rather than rebinding through a generic
   expression placeholder on every iteration.
3. Split quantifier execution into:
   - pure predicate fast path returning `bool`
   - transition-producing path preserving branch information
4. For small immediate sets known when the expression is built, consider
   lowering:
   - `forall({a,b,c}, p)` to `p(a) && p(b) && p(c)`
   - `exists({a,b,c}, p)` to `p(a) || p(b) || p(c)`

Important constraint:

- full construction-time evaluation is only valid when both the set and the
  predicate are independent of runtime context
- in most useful cases we can precompile the predicate shape, but not fully
  evaluate it at construction time

Deliverables:

- no per-evaluation container copy in quantifier/filter runtime loops
- direct quantifier predicate kernel
- dedicated bool fast path for pure quantifiers
- optional eager lowering for small immediate sets with a threshold heuristic
- shared and clearer implementation path for `forall`, `exists`, and `filter`

### Phase 6: Make Branch Expansion Incremental

Avoid eager full materialization of branch cross-products when possible.

Possible options:

- lazy branch iterator/generator
- append-only branch builder with delayed product expansion
- bounded normalization that expands only when required by the engine

This phase should come after explicit branch data exists. Doing it earlier would
make the current lambda design even harder to follow.

Deliverables:

- reduced temporary allocation during large `&&` compositions
- lower peak memory usage for bigger models

### Phase 7: Clean Up the Expression Layer

After the boolean representation is stabilized, revisit `Expression` and
`evaluate_*`.

Targets:

- reduce unnecessary type erasure
- decide whether immediate-vs-callable expression optimization should be used
- remove dead or half-finished abstractions
- simplify lazy evaluation helpers where no longer needed
- reduce internal macro complexity where ordinary named helpers are clearer
- consolidate shared expression plumbing into `src/evaluate.h` or a closely
  related binding utility instead of duplicating extraction/finalization logic in
  multiple places

Potential work items:

- revisit `RawX<R> = std::function<R(Context&)>`
- either use or remove `ErasureExpressionVariant`
- simplify `evaluate_lazy` once boolean operators no longer depend on lambda
  wrappers

Deliverables:

- smaller expression core
- fewer closure layers per operation

## Validation Strategy

For every phase:

- run the full test suite
- add targeted regression tests for any semantic edge case discovered during the
  refactor

## Performance Validation

Performance work should be validated with one fixed benchmark that is run before
the first optimization and after each major phase. The point is to measure the
same workload every time, not to keep changing the benchmark.

### Primary benchmark: branch-heavy boolean normalization

Add a dedicated benchmark sample, for example:

- `benchmarks/boolean_perf.cpp`

The sample should build one expression that is intentionally hard for the
current boolean implementation:

- large `||` groups that produce assignment branches
- large `&&` groups that force branch cross-product normalization
- repeated evaluation over the same model shape to expose allocation and
  dispatch overhead

Recommended benchmark shape:

```cpp
// Pseudocode only.
//
// Build an expression with explicit cross-product expansion pressure:
//   (x++ == 0 || x++ == 1 || ... x++ == 31) &&
//   (y++ == 0 || y++ == 1 || ... y++ == 31) &&
//   (z++ == 0 || z++ == 1 || ... z++ == 31)
//
// This produces many normalized branches and directly exercises:
// - operator||
// - operator&&
// - assignment branch representation
// - engine branch consumption
```

This benchmark is preferred over a tiny microbenchmark because it stresses the
exact area being refactored:

- boolean normalization
- branch composition
- assignment branch storage
- engine-side branch iteration

### Why this benchmark

This workload makes wins visible when the implementation improves in any of the
following ways:

- fewer allocations from replacing `std::function`
- cheaper branch concatenation
- reduced temporary branch materialization
- less recursive/variant-based dispatch inside boolean operators
- better engine consumption of normalized branch data

### How to execute it

Build in a stable optimized configuration:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target boolean_perf
```

Run the exact same binary before and after each optimization step. Use one of
the following approaches:

If `hyperfine` is available:

```sh
hyperfine --warmup 3 './build/benchmarks/boolean_perf'
```

Portable fallback on macOS:

```sh
for i in $(seq 1 10); do
  /usr/bin/time -l ./build/benchmarks/boolean_perf >/dev/null
done
```

The benchmark binary should print enough counters to confirm the workload is the
same across runs, for example:

- number of evaluated branches
- number of accepted states
- total iterations

### Metrics to compare

For the primary benchmark, compare:

- median wall-clock time
- max resident set size or equivalent memory metric
- branch count produced by normalization
- states/transitions produced by the engine

The last two are correctness guards. If runtime improves because the benchmark
accidentally does less work, the result is invalid.

### Before/after reporting format

For each major phase, record results in a small table:

| Version | Benchmark | Median time | Memory | Branches | States |
|:-|:-|:-|:-|:-|:-|
| Baseline | `boolean_perf` | ... | ... | ... | ... |
| After Phase 2 | `boolean_perf` | ... | ... | ... | ... |
| After Phase 4 | `boolean_perf` | ... | ... | ... | ... |
| After Phase 5 | `boolean_perf` + `quantifier_perf` | ... | ... | ... | ... |

The comparison should be made on the same machine, same compiler, same build
type, and without unrelated code changes in the measurement window.

### Secondary benchmarks

After the primary benchmark is in place, add smaller focused workloads only if
needed:

- wide `||` without `&&`
- wide `&&` cross-product without engine traversal
- quantifier-heavy predicate evaluation
- quantifier-heavy branch creation
- a realistic sample such as `paxos`

These are supporting data points. The branch-heavy normalization benchmark above
should remain the canonical before/after test.

Recommended dedicated quantifier benchmark:

- `benchmarks/quantifier_perf.cpp`

Suggested quantifier workload:

- `forall(vec, predicate)` where `vec` is large and stored in context
- `exists(vec, predicate)` with early-hit and late-hit cases
- immediate-set quantifiers such as `forall(std::vector<int>{...}, predicate)`
  to measure the value of construction-time lowering

Run this benchmark before and after Phase 5 specifically. It is the clearest way
to measure whether predicate generation and quantifier specialization actually
reduce runtime work.

### Quantifier perf test: fixed before/after scenario

This benchmark should not be a generic placeholder. It should contain a fixed
set of scenarios and print stable counters so results are comparable across
refactors.

Recommended scenarios:

- `forall` over a large context-owned vector with a predicate that fails only at
  the last element
- `exists` over a large context-owned vector with a predicate that succeeds at
  the first element
- `exists` over a large context-owned vector with a predicate that succeeds at
  the last element
- `forall` over a small immediate vector literal to measure construction-time
  lowering opportunities
- `exists` over a small immediate vector literal to measure construction-time
  lowering opportunities

Recommended concrete sizes:

- large vector: `4096` or `16384` elements
- small immediate vector: `4` to `8` elements
- repeat count inside the benchmark process: high enough to make timing stable,
  for example `1000` to `10000` evaluations per scenario depending on runtime

The benchmark should print:

- scenario name
- element count
- iteration count
- number of `true` results
- total elapsed time per scenario

Example execution:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target quantifier_perf
hyperfine --warmup 3 './build/benchmarks/quantifier_perf'
```

Portable fallback:

```sh
for i in $(seq 1 10); do
  /usr/bin/time -l ./build/benchmarks/quantifier_perf >/dev/null
done
```

What this benchmark should prove:

- borrowing the quantified container is faster than copying it
- a specialized pure-bool quantifier path beats the generic boolean combiner
- early-termination behavior remains fast for `exists` and `forall`
- immediate-set lowering helps when the set is known at construction time

This benchmark should be treated as the canonical quantifier before/after test,
just like `boolean_perf` is the canonical branch-normalization before/after
test.

Suggested benchmark metrics:

- number of allocated branches
- number of heap allocations if measurable
- execution time for init and next evaluation
- peak memory for branch-heavy models

## Risks

### Semantic drift

The current engine behavior has several subtle edge cases. Refactoring without
locking those down first could silently change model behavior.

### Over-optimizing too early

Trying to add lazy branch products before introducing explicit branch data would
increase complexity instead of reducing it.

### Breaking the DSL

The C++ surface syntax is a major value of the project. Internal cleanup should
not leak into user-facing syntax unless there is a clear benefit.

## Recommended Order of Execution

1. Expand tests and document current semantics.
2. Clarify predicate vs transition result semantics.
3. Introduce explicit branch data and port boolean operators.
4. Split `Context` into clearer runtime roles.
5. Specialize quantifier execution and predicate generation.
6. Add incremental branch expansion if benchmarks justify it.
7. Clean up the remaining expression/lazy-evaluation machinery.

## Expected Outcome

If the plan is followed, the boolean subsystem should end up with:

- clearer semantics
- fewer runtime surprises
- less allocation and indirection
- easier debugging
- a better base for future engine optimizations

The biggest win is not a small speedup in one operator. The biggest win is
moving from "boolean logic encoded as opaque nested callables" to "boolean logic
represented as explicit transition data".
