#include <gtest/gtest.h>

#include "../tests/engine_fixture.h"
#include "algorithm.h"
#include "engine.h"
#include "operation.h"
#include "var.h"

namespace die_hard {

using test::EngineFixture;

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/DieHard/DieHard.tla
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

TEST_F(EngineFixture, DieHard) {
  e.createModel<Model>();
  e.run();
  checkStats(Stats{2, 1, 13, 73});
}

}  // namespace die_hard
