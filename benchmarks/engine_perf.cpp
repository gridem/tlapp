#include <gtest/gtest.h>

#include "bench_util.h"
#include "engine.h"
#include "operation.h"
#include "var.h"

namespace engine_perf {
namespace {

struct Model : IModel {
  Boolean init() override { return (x == 1 || x == 10) && (y == 1 || y == 10); }

  Boolean next() override {
    return (x++ == 1000000 + x / 2 - y * 2 || x++ == x) &&
           (y++ == 1000000 + y / 2 || y++ == y);
  }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

}  // namespace

TEST(EnginePerf, Run) {
  auto result = runBench("engine_branchy_run", 1, [] {
    Engine e;
    e.createModel<Model>();
    e.run();
    return benchValue(e.getStats());
  }, BenchConfig{0});
  EXPECT_GT(result.checksum, 0ull);
}

}  // namespace engine_perf
