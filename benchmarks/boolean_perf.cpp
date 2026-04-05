#include <gtest/gtest.h>

#include "bench_util.h"
#include "boolean.h"
#include "operation.h"
#include "var.h"

namespace boolean_perf {
namespace {

Boolean makeAssignOr(Var<int>& var, int width) {
  Boolean expr = var == 0;
  for (int i = 1; i < width; ++i) {
    expr = expr || (var == i);
  }
  return expr;
}

}  // namespace

TEST(BooleanPerf, Run) {
  Var<int> x{"x"};
  Var<int> y{"y"};
  Var<int> z{"z"};

  auto wideOr = makeAssignOr(x, 64);
  auto wideAnd =
      makeAssignOr(x, 16) && makeAssignOr(y, 16) && makeAssignOr(z, 16);

  Context ctx;

  expectBenchPerIteration("boolean_or_assign_64", 5000, 64,
                          [&] { return wideOr(ctx); });

  expectBenchPerIteration("boolean_and_cross_16x16x16", 300, 4096,
                          [&] { return wideAnd(ctx); });
}

}  // namespace boolean_perf
