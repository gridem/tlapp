#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

#include "../tests/engine_fixture.h"
#include "extractor.h"
#include "field.h"
#include "functional.h"
#include "infix.h"
#include "operation.h"
#include "quantifier.h"

namespace two_phase {

using test::EngineFixture;

struct Message : hashable_tag_type {
  explicit Message(int type_, int rm_ = -1) : type(type_), rm(rm_) {}

  int type;
  int rm;

  fields(type, rm)
};

using Messages = std::set<Message>;
using RmState = std::map<int, int>;
using RmSet = std::set<int>;
using StateSet = std::set<int>;

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/transaction_commit/TwoPhase.tla
struct Model : IModel {
  RmState makeStateMap(int state) const {
    RmState result;
    for (auto&& rm : resourceManagers) {
      result[rm] = state;
    }
    return result;
  }

  Boolean send(auto message) {
    return msgs++ == (msgs $cup message);
  }

  Boolean hasMessage(auto message) { return message $in msgs; }

  Boolean tmRcvPrepared(auto rm) {
    return tmState == initState &&
           hasMessage(creator<Message>(preparedType, rm)) &&
           tmPrepared++ == (tmPrepared $cup rm) &&
           unchanged(rmState, tmState, msgs);
  }

  Boolean tmCommit() {
    return tmState == initState && tmPrepared == resourceManagers &&
           tmState++ == committedState && send(Message{commitType}) &&
           unchanged(rmState, tmPrepared);
  }

  Boolean tmAbort() {
    return tmState == initState && tmState++ == abortedState &&
           send(Message{abortType}) && unchanged(rmState, tmPrepared);
  }

  Boolean rmPrepare(auto rm) {
    return at(rmState, rm) == workingState &&
           mutAt(rmState, rm, preparedState) &&
           send(creator<Message>(preparedType, rm)) &&
           unchanged(tmState, tmPrepared);
  }

  Boolean rmChooseToAbort(auto rm) {
    return at(rmState, rm) == workingState &&
           mutAt(rmState, rm, abortedState) &&
           unchanged(tmState, tmPrepared, msgs);
  }

  Boolean rmRcvCommitMsg(auto rm) {
    return hasMessage(Message{commitType}) &&
           mutAt(rmState, rm, committedState) &&
           unchanged(tmState, tmPrepared, msgs);
  }

  Boolean rmRcvAbortMsg(auto rm) {
    return hasMessage(Message{abortType}) &&
           mutAt(rmState, rm, abortedState) &&
           unchanged(tmState, tmPrepared, msgs);
  }

  Boolean typeOk() {
    auto rmStatesOk =
        $A(rm, resourceManagers) { return at(rmState, rm) $in rmStateValues; };
    auto messagesOk = $A(message, msgs) {
      return (get_mem(message, type) == preparedType &&
              get_mem(message, rm) $in resourceManagers) ||
             ((get_mem(message, type) $in tmMessageTypes) &&
              get_mem(message, rm) == -1);
    };
    return rmStatesOk && (tmState $in tmStateValues) &&
           (tmPrepared $in resourceManagers) && messagesOk;
  }

  Boolean consistent() {
    return $A(rm1, resourceManagers) {
      return $A(rm2, resourceManagers) {
        return !(at(rmState, rm1) == abortedState &&
                 at(rmState, rm2) == committedState);
      };
    };
  }

  Boolean init() override {
    return rmState == makeStateMap(workingState) && tmState == initState &&
           tmPrepared == RmSet{} && msgs == Messages{};
  }

  Boolean next() override {
    return tmCommit() || tmAbort() ||
           $E(rm, resourceManagers) {
             return tmRcvPrepared(rm) || rmPrepare(rm) ||
                    rmChooseToAbort(rm) || rmRcvCommitMsg(rm) ||
                    rmRcvAbortMsg(rm);
           };
  }

  std::optional<Boolean> ensure() override { return typeOk() && consistent(); }

  Var<RmState> rmState{"rmState"};
  Var<int> tmState{"tmState"};
  Var<RmSet> tmPrepared{"tmPrepared"};
  Var<Messages> msgs{"msgs"};

  RmSet resourceManagers = {1, 2, 3};
  int initState = 0;
  int workingState = 1;
  int preparedState = 2;
  int committedState = 3;
  int abortedState = 4;
  int preparedType = 10;
  int commitType = 11;
  int abortType = 12;
  StateSet rmStateValues = {workingState, preparedState, committedState,
                            abortedState};
  StateSet tmStateValues = {initState, committedState, abortedState};
  StateSet tmMessageTypes = {commitType, abortType};
};

TEST_F(EngineFixture, TwoPhase) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace two_phase
