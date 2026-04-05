#include <gtest/gtest.h>

#include <set>

#include "../tests/engine_fixture.h"
#include "evaluate.h"
#include "operation.h"
#include "quantifier.h"
#include "var.h"

namespace hour_clock {

using test::EngineFixture;

int nextHour(int hour) { return hour == 12 ? 1 : hour + 1; }

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/SpecifyingSystems/RealTime/HourClock.tla
struct Model : IModel {
  Boolean init() override {
    return $E(h, hours) { return hr == h; };
  }

  Boolean next() override { return hr++ == evaluator_fun(nextHour, hr); }

  std::optional<Boolean> ensure() override { return hr >= 1 && hr <= 12; }

  Var<int> hr{"hr"};
  std::set<int> hours = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
};

TEST_F(EngineFixture, HourClock) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace hour_clock
