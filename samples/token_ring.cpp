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

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/ewd426/TokenRing.tla
struct Model : IModel {
  Boolean createToken() {
    return at(c, 0) == at(c, lastNode) && ((at(c, lastNode) == 0 && mutAt(c, 0, 1)) ||
                                              (at(c, lastNode) == 1 && mutAt(c, 0, 2)) ||
                                              (at(c, lastNode) == 2 && mutAt(c, 0, 0)));
  }

  Boolean passToken(auto node) {
    return node != 0 && at(c, node) != at(c, node - 1) && mutAt(c, node, at(c, node - 1));
  }

  Boolean typeOk() {
    return $A(node, nodes) {
      return at(c, node) >= 0 && at(c, node) < modulo;
    };
  }

  Boolean init() override {
    return c == CounterMap{{0, 0}, {1, 2}, {2, 1}};
  }

  Boolean next() override {
    return createToken() || $E(node, nonZeroNodes) {
      return passToken(node);
    };
  }

  std::optional<Boolean> ensure() override {
    return typeOk();
  }

  std::optional<LivenessBoolean> liveness() override {
    auto c0 = at(c, 0);
    auto c1 = at(c, 1);
    auto c2 = at(c, 2);
    auto uniqueToken =
        (c0 == 0 &&
            ((c1 == 2 && c2 == 2) || (c1 == 0 && c2 == 2) || (c1 == 0 && c2 == 0))) ||
        (c0 == 1 &&
            ((c1 == 0 && c2 == 0) || (c1 == 1 && c2 == 0) || (c1 == 1 && c2 == 1))) ||
        (c0 == 2 &&
            ((c1 == 1 && c2 == 1) || (c1 == 2 && c2 == 1) || (c1 == 2 && c2 == 2)));
    return wf(next()) && eventually(uniqueToken);
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
