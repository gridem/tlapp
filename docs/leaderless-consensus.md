# Leaderless Consensus Models

This note translates the masterless replication variants from the reference
article into this repository's TLA+ and TLA++ style.

Reference article:
[Replicated object, part 7: masterless](https://gridem.blogspot.com/2016/05/replicated-object-part-7-masterless.html)

## Variants

The source material discusses five variants:

1. `Sore`: naive set-based voting.
2. `Calm`: unanimous carry convergence before commit.
3. `Flat`: uniform merge with vote preservation and payload-free commit.
4. `Most`: majority-based carry voting with commit payload propagation in the current model.
5. `Rush`: ordered prefix commitment with generation tracking.

All five are now modeled in this repo.

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

- `MessageIds = {1, 2, 3}` in every module.
- Every variant uses `Nodes = {0, 1, 2}`.
- In `Sore`, `Calm`, `Most`, and `Rush`, a pristine node may originate any not-yet-applied proposal id.
- In `Flat`, proposal generation is reduced to one fixed proposal per node: `0 -> 1`, `1 -> 2`, `2 -> 3`.
- A proposal may be applied only before any vote has been delivered to that node.
- In `Rush`, the analogous rule is stricter: a proposal may be applied only while the node is still in its initial local state.
- Broadcast sends to the other live nodes only.
- The set-based variants model disconnect as an immediate local state update on survivors.
- `Rush` keeps the original "no disconnect handler" behavior and only drops in-flight traffic for failed nodes.

## Safety Checks

The set-based variants (`Sore`, `Calm`, `Flat`, `Most`) check:

- queued message endpoints stay live
- carry sets stay within the applied message ids
- local vote sets stay within the local node set
- live committed nodes agree on the committed carry set

`Rush` uses a different safety predicate:

- queued state-message endpoints stay live
- all carried and committed ids stay within the applied ids
- committed sequences remain pairwise prefix-comparable

That weaker invariant matches the algorithm's progressive prefix-commit design.

## Verification Result

Executable TLA++ sample on the current branch tip:

- `Sore`: expected to fail; serves as the negative baseline and finds a counterexample
- `Calm`: holds under the current finite model
- `Flat`: long-running in the current finite model; no conclusion recorded in this pass
- `Most`: holds under the current finite model
- `Rush`: no final conclusion recorded on the current branch tip

Current failures in the executable model:

- `Sore` (expected negative case)

Open item:

- `Flat` remains long-running; no completed result is recorded on the current branch tip

TLC status:

- TLC verification is a work in progress.

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
