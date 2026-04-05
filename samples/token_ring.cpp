#include <gtest/gtest.h>

#include <map>
#include <set>

#include "../tests/engine_fixture.h"
#include "evaluate.h"
#include "extractor.h"
#include "functional.h"
#include "liveness.h"
#include "operation.h"
#include "quantifier.h"
#include "var.h"

namespace token_ring {

using test::EngineFixture;

using CounterMap = std::map<int, int>;

int incrementMod(int value, int mod) { return (value + 1) % mod; }

bool uniqueToken(const CounterMap& counters, int mod) {
  auto c0 = counters.at(0);
  auto trailing = (c0 + mod - 1) % mod;

  for (size_t split = 0; split <= counters.size(); ++split) {
    bool ok = true;
    for (size_t index = 0; index < split; ++index) {
      if (counters.at(static_cast<int>(index)) != c0) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      continue;
    }
    for (size_t index = split; index < counters.size(); ++index) {
      if (counters.at(static_cast<int>(index)) != trailing) {
        ok = false;
        break;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/ewd426/TokenRing.tla
struct Model : IModel {
  Boolean createToken() {
    return at(c, 0) == at(c, lastNode) &&
           mutAt(c, 0, evaluator_fun(incrementMod, at(c, lastNode), modulo));
  }

  Boolean passToken(auto node) {
    return node != 0 && at(c, node) != at(c, node - 1) &&
           mutAt(c, node, at(c, node - 1));
  }

  Boolean typeOk() {
    return $A(node, nodes) {
      return at(c, node) >= 0 && at(c, node) < modulo;
    };
  }

  Boolean init() override { return c == CounterMap{{0, 0}, {1, 2}, {2, 1}}; }

  Boolean next() override {
    return createToken() ||
           $E(node, nonZeroNodes) { return passToken(node); };
  }

  std::optional<Boolean> ensure() override { return typeOk(); }

  std::optional<LivenessBoolean> liveness() override {
    return wf(next()) && eventually(evaluator_fun(uniqueToken, c, modulo));
  }

  Var<CounterMap> c{"c"};

  std::set<int> nodes = {0, 1, 2};
  std::set<int> nonZeroNodes = {1, 2};
  int modulo = 3;
  int lastNode = 2;
};

TEST_F(EngineFixture, TokenRing) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace token_ring
