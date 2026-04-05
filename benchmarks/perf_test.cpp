#include <gtest/gtest.h>

#include "engine.h"
#include "operation.h"
#include "var.h"

namespace perf_test {

struct Model : IModel {
  Boolean init() override { return (x == 1 || x == 10) && (y == 1 || y == 10); }

  Boolean next() override {
    return (x++ == 1000000 + x / 2 - y * 2 || x++ == x) &&
           (y++ == 1000000 + y / 2 || y++ == y);
  }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

TEST(PerfTest, Run) {
  Engine e;
  e.createModel<Model>();
  e.run();
}

}  // namespace perf_test
