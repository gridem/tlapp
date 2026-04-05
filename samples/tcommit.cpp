#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

#include "../tests/engine_fixture.h"
#include "extractor.h"
#include "functional.h"
#include "infix.h"
#include "operation.h"
#include "quantifier.h"

namespace tcommit {

using test::EngineFixture;

using RmState = std::map<int, int>;
using RmSet = std::set<int>;
using StateSet = std::set<int>;

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/transaction_commit/TCommit.tla
struct Model : IModel {
  RmState makeStateMap(int state) const {
    RmState result;
    for (auto&& rm : resourceManagers) {
      result[rm] = state;
    }
    return result;
  }

  Boolean canCommit() {
    return $A(rm, resourceManagers) { return at(rmState, rm) $in commitStates; };
  }

  Boolean notCommitted() {
    return $A(rm, resourceManagers) { return at(rmState, rm) != committed; };
  }

  Boolean prepare(auto rm) {
    return at(rmState, rm) == working &&
           mutAt(rmState, rm, prepared);
  }

  Boolean decide(auto rm) {
    return (at(rmState, rm) == prepared && canCommit() &&
            mutAt(rmState, rm, committed)) ||
           (at(rmState, rm) $in undecidedStates && notCommitted() &&
            mutAt(rmState, rm, aborted));
  }

  Boolean typeOk() {
    return $A(rm, resourceManagers) { return at(rmState, rm) $in rmStates; };
  }

  Boolean consistent() {
    return $A(rm1, resourceManagers) {
      return $A(rm2, resourceManagers) {
        return !(at(rmState, rm1) == aborted &&
                 at(rmState, rm2) == committed);
      };
    };
  }

  Boolean init() override { return rmState == makeStateMap(working); }

  Boolean next() override {
    return $E(rm, resourceManagers) { return prepare(rm) || decide(rm); };
  }

  std::optional<Boolean> ensure() override { return typeOk() && consistent(); }

  Var<RmState> rmState{"rmState"};

  RmSet resourceManagers = {1, 2, 3};
  int working = 0;
  int prepared = 1;
  int committed = 2;
  int aborted = 3;
  StateSet rmStates = {working, prepared, committed, aborted};
  StateSet commitStates = {prepared, committed};
  StateSet undecidedStates = {working, prepared};
};

TEST_F(EngineFixture, TCommit) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace tcommit
