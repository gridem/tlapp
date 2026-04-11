# TLA++

This repository contains the code that utilizes ideas around TLA+ (Temporal Logic of Actions) in C++.

The main idea is to use the power of C++ templates and enable using C++ for TLA+ verification of algorithms.

The author's aim was to fully understand how TLA+ works and show that C++ is able to follow the same approach and may use the expressiveness of the language.

Longer-form notes, benchmarks, and design plans live under
[`docs/`](docs/README.md).

## Quick Example

Let's see how TLA+ can be represented in TLA++.

### TLA+

```
Init == /\ big = 0 
        /\ small = 0

FillSmallJug  == /\ small' = 3 
                 /\ big' = big

FillBigJug    == /\ big' = 5
                 /\ small' = small

EmptySmallJug == /\ small' = 0
                 /\ big' = big

EmptyBigJug   == /\ big' = 0
                 /\ small' = small

SmallToBig == /\ big'   = Min(big + small, 5)
              /\ small' = small - (big' - big)

BigToSmall == /\ small' = Min(big + small, 3)
              /\ big'   = big - (small' - small)

Next ==  \/ FillSmallJug
         \/ FillBigJug
         \/ EmptySmallJug
         \/ EmptyBigJug
         \/ SmallToBig
         \/ BigToSmall

NotSolved == big # 4
```

### TLA++

Corresponding code in C++ (excerpt from `samples/die_hard.cpp`):

```cpp
struct Model : IModel {
  Boolean init() override {
    return big == 0 && small == 0;
  }

  Boolean next() override {
    auto fillSmallJug = small++ == 3 && big++ == big;
    auto fillBigJug = small++ == small && big++ == 5;
    auto emptySmallJug = small++ == 0 && big++ == big;
    auto emptyBigJug = small++ == small && big++ == 0;
    auto bigNext = min(big + small, 5);
    auto smallToBig = big++ == bigNext && small++ == small - (bigNext - big);
    auto smallNext = min(big + small, 3);
    auto bigToSmall = small++ == smallNext && big++ == big - (smallNext - small);
    return fillSmallJug ||
           fillBigJug ||
           emptySmallJug ||
           emptyBigJug ||
           smallToBig ||
           bigToSmall;
  }

  std::optional<Boolean> stop() override {
    return big++ == 4;
  }

  Var<int> big{"big"};
  Var<int> small{"small"};
};
```

You see that there is a clear 1:1 mapping between TLA+ syntax and TLA++ syntax which is C++ with **tagged expressions** technique.

### Sample Output (die_hard)

```
GLOG_logtostderr=1 ./build/samples/die_hard

Trace: [big=0, small=0]
Trace: [big=5, small=0]
Trace: [big=2, small=3]
Trace: [big=2, small=0]
Trace: [big=0, small=2]
Trace: [big=5, small=2]
Trace: [big=4, small=3]
```

Each step is a valid jug operation under the model's `next` transitions,
starting from the initial state. The final state satisfies the stop condition
(`big' = 4`), so the engine terminates after producing a valid path to a
4‑gallon result.

## Leaderless Consensus

This repo also contains a larger multi-model example for leaderless consensus
under `leaderless_consensus/`.

- Executable TLA++ models live in:
  `leaderless_consensus/sore.cpp`, `leaderless_consensus/calm.cpp`,
  `leaderless_consensus/flat.cpp`, `leaderless_consensus/most.cpp`, and
  `leaderless_consensus/rush.cpp`
- Matching TLA+ specs live in `leaderless_consensus/specs/`
- The overview, assumptions, and current status are documented in
  [`docs/leaderless-consensus.md`](docs/leaderless-consensus.md)

The modeled variants are:

1. `Sore`: naive set-based voting baseline
2. `Calm`: unanimous carry convergence before commit
3. `Flat`: uniform merge with vote preservation and payload-free commit
4. `Most`: majority-based carry voting with commit payload propagation
5. `Rush`: the most advanced variant, using ordered prefix commitment with
   generation tracking; the current reduced model omits disconnects, so it does
   not need timeout-based failure handling

Build and run the executable sample with:

```sh
cmake --build build/rel --target leaderless-consensus
./build/rel/samples/leaderless-consensus --gtest_brief=1
```

## Novel Ideas

### Domain Language

The implementation itself is pretty complicated and hard to understand sometimes. The reason is that I wanted to reduce the amount of boilerplate code and introduce a new kind of language to simplify programming. This language comes from extensive use of macros that allows to significantly improve the density and soundness of the language while keeping genericity and performance.

### Tagged Expressions

On top of that there is new C++ template technique that is **tagged template expressions**. Tagged expressions allow to use conditional constructions in a very easy form and it's pretty similar to template pattern matching.

### Logic

A lot of work and complexity was done to improve boolean representation and
logic, especially in the area of quantifiers. The external DSL stayed the same,
but the internal model was recently simplified around explicit branch data and
role-specific binding:

1. `Boolean` expressions can now be bound into explicit roles such as
   `PredicateMode`, `InitMode`, and `NextMode`.
2. Branch-producing logic is represented as explicit ordered branch data rather
   than nested `std::function` chains.
3. Quantifiers reuse generated predicate expressions, borrow their input sets,
   and use a direct bool fast path when possible.

This keeps the model code declarative while making the runtime easier to reason
about and substantially faster on heavy boolean and quantifier workloads.

### Variadics and Forwarding

There are different language limitations in how the parameters are wrapped/unwrapped and forwarded to further functions. One thing that makes the code complex is that sometimes I want to use functions and macros uniformly. In that case forwarding may not work as expected. Thus in order to forward I introduce a new forwarding mechanism that seems weird.

All the macros are built having in mind a set of unbound parameters to have the most generalized form of expressions.

## Syntax

The table below summarizes the mapping between TLA+ and TLA++:

| Description | TLA+ | TLA++ |
|-:|:-:|:-:|
| Variable declaration | `VARIABLES a` | `Var<int> a{"a"};` |
| Init section | `Init == ...` | `Boolean init() { ... }` |
| Next section | `Next == ...` | `Boolean next() { ... }` |
| Assignment/Definition | `a == b` | `auto a = b;` |
| Prime/Next | `a'` | `a++` |
| Arithmetics | `a + b * 3 - 4` | `a + b * 3 - 4` |
| And | `/\ a /\ b` | `a && b` |
| Or | `\/ a \/ b` | `a \|\| b` |
| Comparison/Equality | `a = b` | `a == b` |
| Set: in | `x \in y` | `x $in y` |
| Set: not in | `x \notin y` | `!(x $in y)` |
| Set: cup | `x \cup y` | `x $cup y` |
| Set: cap | `x \cap y` | `x $cap y` |
| Set: diff | `x \ y` | `x $diff y` |
| Set: sym diff | N/A (not standard in TLA+) | `x $sym_diff y` |
| For all | `\A e \in set : ...` | `$A(e, set) { ... }` |
| Exists | `\E e \in set : ...` | `$E(e, set) { ... }` |
| Set filter | `{e \in set : ... }` | `$if(e, set) { ... }` |
| Unchanged | `UNCHANGED x` | `unchanged(x)` |
| Eventually | `<> P` | `eventually(p)` |
| Weak fairness | `WF_vars(A)` | `wf(a)` |
| Strong fairness | `SF_vars(A)` | `sf(a)` |

The rest of the operations can be found in samples and tests.

### Set semantics

- Set operations (`$cup`, `$cap`, `$diff`, `$sym_diff`) are implemented via
  `std::set_*` algorithms, so inputs must be sorted containers (e.g. `std::set`
  or sorted `std::vector`).
- `$in` checks membership for element-in-set, and behaves like subset
  (`std::includes`) when the left operand is a set.

## Engine

TLA++ engine contains different parts and concepts that requires deep understanding and reasons of the implementation.

Before going into details let's discuss high level idea of the engine.

### Overview

The main approach is the following:

1. User defines a model by specifying `init` and `next` methods.
1. User instantiates engine and creates the model instance inside.
2. The methods `init` and `next` return `Boolean` expressions that describe
   predicates and transition logic.
3. Before execution the engine binds those expressions into an explicit role:
   init transition, next transition, or pure predicate
   (`skip` / `ensure` / `stop`).
4. The engine first registers the variables by using `init` expression. This
   expression contains a set of variables that lazily register and initialize a
   state.
5. `Boolean` logic contains not just a single state, but a set of possible
   branches. For now you don't need to understand how and why, just remember
   that any `Boolean` may represent a set of outcomes that engine tries to
   evaluate and iterate through state-by-state.
6. `next` method allows to generate new states based on a current state
   allowing temporal logic to go through all possible combinations and allowed
   states according to user logic. The main idea here is that logic can be
   transformed into state machine and outcome of that state machine is not just
   a single state, but a set of possible states that may happen.

The power of temporal logic is that you use the checks in order to specify all possible outcomes that system may have. Those behaviors could be good scenarios according to expectations, and also some corner cases and error scenarios that is also possible. Iterating through all possible combinations allows the algorithm to check the invariants regardless of "good" or "bad" conditions. This is an essential core of verification algorithms.

At the same time, temporal logic is not a natural way of engineer thinking. The reason is that state machine outcome is just a single state, not a set of possible states. That's why the temporal logic is sometimes hard to understand. But if you know how it works exactly, it becomes pretty trivial to figure out the reasoning and use that power.

At the same time, temporal logic is pretty generic and doesn't allow you to do actual verification since result of verification depends on the model that you put as a part of your TLA+ or TLA++ spec. Spec itself must contain possible failure scenarios and if you miss some failure scenarios that may happen in a real world execution, your "validated" algorithm may fail there in reality. So having good enough failure model is a critical aspect for proper validation of the algorithm.

The most interesting thing is that the real world behaves in a Byzantine way and TLA+ cannot check and validate it without formal verification procedures that is outside the current topic.

Below you can find some important details about engine, types, and other concepts that is used in the codebase.

### Value

Value is a type erasure container that holds a typed variable. The idea is to store typed values in a uniform way based on runtime `init` procedure. Since the engine cannot know the set of variables in compile time, thus we need to store it in a runtime and provide uniform way of access. For that purpose to manipulate the data type erasure is applied in order to store those values inside the `Context` and engine `states`.

### Context

Context is a current execution context that allows to extract the current variables, assign next variables, update states etc. So it holds all variables for further evaluation and assignment.

There are several important operations:

1. `vars` and `nexts`: stores all states of variables as a current state and next state. The state is a vector of `Value`s.
1. Register: registers new variable (see variable description below). It creates new value for both `vars` and `nexts`.

### Binding Modes

One of the recent internal type-model changes is that the same DSL expression
can be bound into different execution roles before evaluation:

1. `bind(expr, PredicateMode{})` is used for pure checks and must produce a
   plain `bool`.
2. `bind(expr, InitMode{})` preserves init-state assignment semantics.
3. `bind(expr, NextMode{})` preserves next-state transition semantics.

This keeps the DSL stable while making the engine semantics more explicit.

### Variables

Variables are tagged expressions that allows to extract the state from the context using a reference. When variable is registered it stores the reference to the state - descriptor. Descriptor specifies the position (index) in a state (where state is a vector of values). Thus, variable is just a reference to a position inside a context.

If you apply `operator++` to variable it returns expression that refers not to current state, but to the next state.

Variables must specify the name for better output. The name itself doesn't matter for engine. You may put empty name or the same name for all variables, but output could be hard to understand in that way.

### Boolean Logic

Let's consider the logic by examples.

Given `Var<int> x{"x"}`. Here we consider that we use `init()` method implementation that returns the value from the table. The description will describe what happens in the engine and how the engine interprets it.

| Example | Description |
|:-:|-|
| `x > 2` | This is a simple condition and evaluates to true or false depending on the value of `x`. For `init()` phase it doesn't make any sense since `x` must be initialized first before the comparison. |
| `x == 2` | It seems that it's a comparison of `x` with `2`, but it's not. It evaluates to a state assignment: after execution the engine will assign `x` to `2` here. |
| `x == 2 \|\| x == 3` | The outcome of it from engine point of view will be 2 states: with `x = 2` and `x = 3`. |

The last example seems nontrivial and requires additional explanation. The
boolean logic may contain not just an outcome which is `true` or `false`, but
also a set of outcomes and a set of possible states for a state machine.

In the example above there will be 2 initial states with `x = 2` and `x = 3`.
From the implementation perspective the boolean is represented as a
`std::variant` of 2 types:

1. `bool` type. It evaluates to a corresponding `true` or `false`. There are
   shortcuts for logical "and" and "or" operations similar to C++ shortcuts.
2. `LogicResult` type. This is a vector of `BranchResult`, where each branch is
   an ordered conjunction of checks and assignments that may create a new state
   for the engine.

So in the example above the normalized result contains 2 branches, one for
`x = 2` and one for `x = 3`, that engine later interprets as 2 separated
initial states.

### Boolean Operators

The implementation normalizes booleans in expressions. Normalized expression is
either `true` / `false` or `LogicResult`, where `LogicResult` is a vector of
ordered branches.

Operator "or" does the following:

1. If the first argument is `true` -> all the expression is `true`.
2. If the first argument is `false` -> result is second expression.
3. If both are `LogicResult`, it merges them by concatenating their branches.

Operator "and" is more complex:

1. If the first argument is `true` -> result is second expression.
2. If the first argument is `false` -> all the expression is `false`.
3. If both are `LogicResult`, it iterates through all possible combinations to
   normalize them into a new vector of branches.

It's easier to understand this based on example. Given `Var<int> x{"x"}` and `Var<int> y{"y"}`. Here we consider that we use `init()` method implementation:

| Example | Description |
|:-:|-|
| `x == 2 && y == 1` | It creates a single state with _{x, y} = {2, 1}_. |
| `x == 0 && y == 0 \|\| x == 1 && y == 3` | It creates 2 states with _{0, 0}_ and _{1, 3}_. |
| `(x == 0 \|\| x == 3) && (y == 1 \|\| y == 2)` | The outcome is 4 states: _{0, 1}_, _{0, 2}_, _{3, 1}_, _{3, 2}_. |

So the last "and" operation creates all possible variants across all "or" conditions. Thus the overall normalized result will be equivalent to the following statement:

`x == 0 && y == 1 || x == 0 && y == 2 || x == 3 && y == 1 || x == 3 && y == 2`

> __Note__. "And" operation `&&` has higher precedence than "or" operation `||`.

### Quantifiers

There are 2 possible quantifiers:

1. Forall (TLA+: `\A`, TLA++: `$A`). It's equivalent to apply "and" operation for a set of expressions.
2. Exists (TLA+: `\E`, TLA++: `$E`). It's equivalent to apply "or" operation for a set of expressions.

Example:

```cpp
// Given vec contains {1, 2} elements.
auto e1 = $A(i, vec) { return i > 1; };
// e1 equivalent to 1 > 1 && 2 > 1 which is false.

auto e2 = $E(i, vec) { return i > 1; };
// e2 equivalent to 1 > 1 || 2 > 1 which is true.
```

Implementation note:

1. The predicate lambda is converted into an expression once and then reused
   across element iteration.
2. Pure predicate quantifiers use a direct bool loop with short-circuiting.
3. Assignment-producing quantifiers still produce `LogicResult` branches and
   combine them with the same boolean branch machinery.
4. `filter` uses the same element-binding pattern as quantifiers.

### Iterations

Engine goes through all possible states using `init` and `next` methods that contains boolean logic.

1. `init` provides initial set of states.
2. `next` iterates through all possible combinations.

If `next` generates the same state again, it just discards it because boolean logic doesn't have any dependency on the history, only current state is needed for evaluation. It corresponds to what we have in reality when we program our systems: only current state is used to generate next state.

`next` method must use next variables by applying `operator++` to the variable. E.g. `x++ == x + 1` assigns the new state to variable `x` that adds `1` to the current state.

The termination condition is when all possible states and combinations were considered and there is no new generated state.

The model may contain additional optional methods:

```cpp
// Skips that state.
virtual std::optional<Boolean> skip();
// Ensures state invariant.
virtual std::optional<Boolean> ensure();
// Stops on that state.
virtual std::optional<Boolean> stop();
// Liveness obligations.
virtual std::optional<LivenessBoolean> liveness();
```

So we could have additional engine behavior to modify the logic:

1. `skip` allows to skip that state based on a boolean logic. You can use both current and next variables in order to skip.
2. `ensure` checks for invariants on each iterations. Useful to enforce correctness for each state. You can use both current and next variables in order to validate.
3. `stop` allows to terminate the iterations earlier based on a condition. Pretty similar to `!ensure()`, but it's a valid stop condition that is treated separately. You can use both current and next variables in order to stop.
4. `liveness` adds temporal obligations that are checked on the explored graph after reachability is finished.

### Liveness

The current liveness API supports conjunctions of:

1. `eventually(p)` which corresponds to `<> P`.
2. `wf(a)` which corresponds to weak fairness of an action.
3. `sf(a)` which corresponds to strong fairness of an action.

Example:

```cpp
Boolean advance() { return x < 3 && x++ == x + 1; }

std::optional<LivenessBoolean> liveness() override {
  return wf(advance()) && eventually(x == 3);
}
```

There are 2 important details here:

1. `wf(...)` and `sf(...)` expect an action formula, so they are evaluated in the same next-state style as `next()`.
2. `eventually(...)` expects a state predicate, so it is evaluated in predicate/check mode without assignments.

Liveness conjunction is explicit:

```cpp
return wf(a()) && wf(b()) && eventually(chosen());
```

This is not the same as:

```cpp
return wf(a() || b()) && eventually(chosen());
```

The first form requires fairness for `a` and `b` separately. The second form requires fairness only for the combined action `a \/ b`.

The engine checks liveness after the full reachable graph is built. `eventually(p)` fails if there is a reachable infinite behavior where `p` never becomes true. Deadlocks are treated as stuttering self-loops for liveness, so a deadlocked state with `!p` also violates `eventually(p)`.

### Engine semantics

- **Init coverage:** every variable must be assigned in the first init branch;
  otherwise initialization fails (e.g., `VarInitError`/`VarValidationError`).
- **Binding roles:** engine evaluates `init` in `InitMode`, `next` in
  `NextMode`, and `skip` / `ensure` / `stop` in `PredicateMode`.
- **Check mode:** `skip/ensure/stop` are evaluated in check mode, so `==` is a
  pure comparison (no assignment side effects), and branch-producing results
  are rejected there.
- **Liveness mode:** `eventually(...)` is evaluated in predicate/check mode,
  while `wf(...)` and `sf(...)` are evaluated as next-state actions.
- **Full graph for liveness:** liveness is checked after exploration over the
  admitted state graph, not during the main state-generation loop.
- **Deadlocks:** for liveness only, deadlocks are treated as stuttering
  self-loops.
- **`stop` incompatibility:** `stop()` cannot be used together with
  `liveness()`, because early termination would make the liveness result
  unsound.
- **Quantifiers on empty sets:** `forall` returns `true`, `exists` returns
  `false`.
- **Trace output:** traces are produced when stopping or on invariant failure,
  and also on liveness failure. They show the predecessor chain of states
  leading to that stop or counterexample cycle.

### Enabling traces

Traces are emitted when a `stop` condition is hit, when an `ensure` invariant
fails, or when a liveness condition fails. To see them, enable glog output (for
example):

```
GLOG_logtostderr=1 ./build/samples/die_hard
```

You can also use glog flags (e.g. `--logtostderr=1`) if you prefer runtime
arguments.

### Performance Notes

Recent optimization work focused on keeping the DSL stable while improving the
internals behind `Boolean`, quantifiers, and branch normalization. The current
implementation keeps explicit branch data, borrows quantified containers, and
special-cases pure predicate quantifiers.

The main benchmark summary so far is:

1. heavy branch cross-products are much faster than the original baseline
2. assignment-producing quantifiers are faster than the original baseline
3. wide `||` chains are much closer to baseline than before, but still leave
   some room for improvement

Detailed numbers and scenarios are recorded in
[`docs/bench.md`](docs/bench.md).

## Implementation Details

The code contains many unique items and technologies. Let's deep dive into them and explain in detail.

### Tagged Expressions

There is a well known expression templates technique that allows having a complex expression assignment before going into the execution of those assignments. Basically, the main idea for the expression templates technique is to create an expression as a set of actions that can be executed later. This allows providing different optimizations during expression creation since it's possible to check the sequence of expressions using either templates and type information at compile time or at runtime. This is a pretty powerful but complex technique that requires knowing C++ templates.

The problem with expression templates is that it may require some pattern matching and checks for a specific template in order to find the corresponding implementation. Template pattern matching is not incorporated into the language itself, thus it was important to build something similar and sophisticated.

E.g. one of the important use-case is to use overloaded global operators like `+`, `-`, `==`, `&&` etc. The idea is that the left hand item can be `int` thus it's not possible to bind the implementation to a specific class: it must be global. And what we want is to define global `+` operation in a way that only allows to capture and match our unique expressions.

For that purpose the following approach is used:

1. For any class that we want to pattern match we create a tag and derive the class from that tag.
2. Tag is just an empty class.
3. When we need to have a pattern match, we check whether the class is derived from the tag we are looking for.

See example:

```cpp
// Can be used in expression context
// Declares the tag with name "expression"
DEFINE_TAG(expression)

// Defines the global function that accepts 2 parameters
// if any of them are expressions (tagged by "expression" tag).
fun_if(operator==, is_any_of(is_expression, T_l, T_r), l, r) { ... }
```

In that example `==` will be used only in the case if ANY of the type is tagged by `expression` tag.

If you need to test that all types is tagged by a specific tag you can use `is_all_of` instead of `is_any_of`.

### Macros

Let's talk about advanced use of the language. You can see a lot of examples in the file "macro.h". Here are just several important use-cases.

```cpp
// tname: Declares set of templates.
tname(T) => template<typename T>
tname(T, U) => template<typename T, typename U>
tname(A, B, C) => template<typename A, typename B, typename C>
tname(T, ...V) => template<typename T, typename ...V>
```

```cpp
// tname_if: Declares set of templates and checks for a specific condition for pattern matching.
tname_if(Condition, T) => template<typename T, use_if(Condition)>
// where use_if(Condition) is defined as
#define use_if(D_bool) std::enable_if_t<D_bool, int> = 0

// Example: defines a constructor of ErasureExpression only if not untyped expression for T.
tname_if(!is_expression_untyped<T>, T) ErasureExpression(T&& t)
```

```cpp
// fun: Defines the function with templates
fun(X, a, b) =>
    template<typename T_a, typename T_b>
    decltype(auto) X(T_a&& a, T_b&& b)
```

```cpp
// funs: Defines the function with templates where the last one is variadic.
funs(X, a, b) =>
    template<typename T_a, typename... T_b>
    decltype(auto) X(T_a&& a, T_b&& b...)
```

```cpp
// fun_if: Defines the function if it satisfies the condition.
// E.g. for pattern matching of tagged expressions.
fun_if(X, Condition, a, b) =>
    template<typename T_a, typename T_b, use_if(Condition)>
    decltype(auto) X(T_a&& a, T_b&& b)
```

```cpp
// as_lam: converts template function into generic lambda as an object
// that can be passed as a parameter for higher order functions.
fun(myBinaryFun, a, b) { ... }
fun(highOrderFun, binaryFun) { ... }

highOrderFun(as_lam(myBinaryFun));
```

See some other useful macro with comments in a file "macro.h".

### Infix Operators

Language itself doesn't allow to use infix operators. But if we really want to use it no one can prevent us to do that.

Any infix operator is represented as the following:

```cpp
a $infix b =>
    a % infix_tag % b
```

Here `infix_tag` is an instance of `infix_tag_type` that defined specifically for the `$infix` operation.

Then we use pattern matching for operator% 2 times: for the first `a % infix_tag` operation, and for the second `... % b` operation.

```cpp
// First call: according to macro expansion
// it will be called by `t % infix_tag_type{}` or `t % infix_tag`
tname(T) let operator%(T&& t, infix_tag_type) {
    // To simplify the next line, we mix value t with infix_tag.
    // Thus for the next expression we can deduce that it's
    // infix expression and can execute the corresponding function.
    return detail::prepare(fwd(t)) & infix_tag;
}

// Second call: check that left argument is mixed with infix_tag from the first call:
fun_if(operator%, is_infix<T_l>, l, r) {
    // It just executes the function D_fun that we specify.
    // Here the actual execution is applied to the args.
    return evaluator_fun(D_fun, l, r);
}
```
