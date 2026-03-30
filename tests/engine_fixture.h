#include <gtest/gtest.h>

#include "engine.h"

namespace test {

struct EngineFixture : ::testing::Test {
  void checkStats(const Stats& stats) {
    auto& engineStats = e.getStats();
    EXPECT_EQ(stats.init.vars, engineStats.init.vars);
    EXPECT_EQ(stats.init.states, engineStats.init.states);
    EXPECT_EQ(stats.loop.states, engineStats.loop.states);
    EXPECT_EQ(stats.loop.transitions, engineStats.loop.transitions);
  }

  Engine e;
};

}  // namespace test
