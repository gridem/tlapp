# Leaderless Consensus Models

This note translates the masterless replication variants from the reference
article into this repository's TLA+ and TLA++ style.

Reference article:
[Replicated object, part 7: masterless](https://gridem.blogspot.com/2016/05/replicated-object-part-7-masterless.html)

## Variants

The source material discusses five variants:

1. `Sore`: naive set-based voting.
2. `Calm`: unanimous proposal convergence before commit.
3. `Flat`: uniform merge with vote preservation and payload-free commit.
4. `Most`: majority-based proposal voting with commit payload propagation in the current model.
5. `Rush`: the most advanced variant, using ordered prefix commitment with
   generation tracking. The current reduced model omits disconnects, so it does
   not need timeout-based failure handling.

All five are now modeled in this repo.

Detailed per-algorithm notes live under
[`docs/leaderless-consensus/`](leaderless-consensus/README.md).

## Files

- `leaderless_consensus/common.h`: shared TLA++ helper types and utilities
- `leaderless_consensus/sore.cpp`: executable TLA++ model for the naive variant
- `leaderless_consensus/calm.cpp`: executable TLA++ model for the calm variant
- `leaderless_consensus/flat.cpp`: executable TLA++ model for the flat variant
- `leaderless_consensus/most.cpp`: executable TLA++ model for the majority variant
- `leaderless_consensus/rush.cpp`: executable TLA++ model for the prefix variant
- `leaderless_consensus/specs/Sore.tla`
- `leaderless_consensus/specs/Calm.tla`
- `leaderless_consensus/specs/Flat.tla`
- `leaderless_consensus/specs/Most.tla`
- `leaderless_consensus/specs/Rush.tla`

## Modeling Assumptions

The executable and TLC models use small finite abstractions:

- `MessageIds = {10, 11, 12}` in every module.
- Every variant uses `Nodes = {0, 1, 2}`.
- In `Sore`, `Calm`, `Most`, and `Rush`, a pristine node may originate any not-yet-proposed proposal id.
- In `Flat`, proposal generation is reduced to one fixed proposal per node: `0 -> 10`, `1 -> 11`, `2 -> 12`.
- A proposal may be proposed only before any vote has been delivered to that node.
- In `Rush`, the analogous rule is stricter: a proposal may be proposed only while the node is still in its initial local state.
- Broadcast sends to the other live nodes only.
- The set-based variants model disconnect as an immediate local state update on survivors.
- `Rush` currently omits disconnect transitions while the ordering model is being reduced.

## Safety Checks

The set-based variants (`Sore`, `Calm`, `Flat`, `Most`) check:

- queued message endpoints stay live
- proposal sets stay within the proposed message ids
- local vote sets stay within the local node set
- live committed nodes agree on the committed proposal set

`Rush` uses a different safety predicate:

- queued state-message endpoints stay live
- all proposed and committed ids stay within the proposed ids
- committed sequences remain pairwise prefix-comparable

That weaker invariant matches the algorithm's progressive prefix-commit design.

## Liveness Checks

`Calm`, `Flat`, `Most`, and `Rush` also include a liveness check in both the executable model and
the TLA+ spec:

- `Calm` now uses separate safety and liveness models: safety keeps the full
  disconnect space, while liveness requires some node to commit a non-empty
  proposal set under majority-preserving disconnects
- `Flat` also uses separate safety and liveness models: safety keeps the full
  disconnect space, while liveness requires some node to commit a non-empty
  proposal set under majority-preserving disconnects
- `Rush` uses separate safety and liveness models over the same reduced
  no-disconnect transition relation, and liveness requires some node to commit
  a non-empty prefix
- `Calm`, `Flat`, and `Most` use action-level weak fairness on `ProposeAny`
  and `DeliverAnyVote`
- `Rush` uses action-level weak fairness on `ProposeAny` and `DeliverAnyState`

## Verification Result

Executable TLA++ sample on the current branch tip:

- `Sore`: expected to fail; serves as the negative baseline and finds a counterexample
- `Calm`: holds under the current finite model, including the liveness check
- `Flat`: holds under the current reduced executable model, including the liveness check
- `Most`: holds under the current finite model, including the liveness check
- `Rush`: holds under the current reduced executable model, including the liveness check

Current failures in the executable model:

- `Sore` (expected negative case)

TLC status:

- the TLA+ specs were rechecked against the current executable models and updated
  where they had drifted, notably on disconnect side effects and `Rush`
  generation handling
- completed TLC results on the current branch tip:
  - `Sore` safety: fails, as expected
  - `Calm` safety: passes
  - `Calm` liveness: passes
  - `Flat` safety: passes
  - `Flat` liveness: passes
  - `Most` safety: passes
  - `Most` liveness: passes
  - `Rush`: still in progress
    - the checked-in spec starts cleanly, but TLC spends too much time per
      state in promise normalization and hot-path type checks
    - a tested spec-only rewrite makes safety complete in about `24.1s`
      (`522040` generated, `212530` distinct), but that rewrite is not checked
      in yet
    - liveness remains the outstanding heavy case because TLC's temporal phase
      is still much slower than the executable checker
    - the current improvement plan is documented in
      [`docs/plans/rush-tlc-performance.md`](plans/rush-tlc-performance.md)

## Timing Comparison

Recent checks from `build/rel` and TLC:

| Variant | TLA++ safety | TLC safety | TLA++ liveness | TLC liveness |
| --- | ---: | ---: | ---: | ---: |
| `Calm` | 3.6s | 8.4s | 5.5s | 22.6s |
| `Flat` | 28.0s | 46.7s | 45.5s | 161.3s |
| `Most` | 3.3s | 11.7s | 3.6s | 69.5s |
| `Rush` | 12.9s | in progress | 22.3s | in progress |

Current takeaway:

- in the completed comparisons on the current branch tip, the executable TLA++
  models are consistently faster than TLC
- the gap is most pronounced for liveness checks, where the executable checker
  avoids a large fraction of TLC's temporal-analysis cost

Additional current points:

- `Sore` executable safety fails as expected in under `0.1s`
- `Sore` TLC safety fails in about `1.1s`
- `Rush` TLC `LiveSpec` starts cleanly and remains CPU-active, but did not
  finish in the observed run window
- `Rush` TLC safety is slow in the checked-in spec, but a tested local rewrite
  reduced it to about `24.1s`; that work is tracked in the plan note above

## Commands

Build and run the executable models:

```sh
cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/rel --target leaderless-consensus
./build/rel/samples/leaderless-consensus --gtest_brief=1
```

If CMake does not find GoogleTest automatically, add:

```sh
-DGTest_DIR=<path-to-GTest-config>
```

Run TLC:

```sh
JAVA_BIN=${JAVA_BIN:-java}
TLA_TOOLS_JAR=${TLA_TOOLS_JAR:-tla2tools.jar}

$JAVA_BIN -cp "$TLA_TOOLS_JAR" \
  tlc2.TLC -cleanup -config leaderless_consensus/specs/Sore.cfg \
  leaderless_consensus/specs/Sore.tla

$JAVA_BIN -cp "$TLA_TOOLS_JAR" \
  tlc2.TLC -cleanup -config leaderless_consensus/specs/Calm.cfg \
  leaderless_consensus/specs/Calm.tla

$JAVA_BIN -cp "$TLA_TOOLS_JAR" \
  tlc2.TLC -cleanup -config leaderless_consensus/specs/Flat.cfg \
  leaderless_consensus/specs/Flat.tla

$JAVA_BIN -cp "$TLA_TOOLS_JAR" \
  tlc2.TLC -cleanup -config leaderless_consensus/specs/Most.cfg \
  leaderless_consensus/specs/Most.tla

$JAVA_BIN -cp "$TLA_TOOLS_JAR" \
  tlc2.TLC -cleanup -config leaderless_consensus/specs/Rush.cfg \
  leaderless_consensus/specs/Rush.tla
```
