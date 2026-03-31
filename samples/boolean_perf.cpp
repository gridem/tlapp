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

uint64_t branchCount(const BooleanResult& result) {
  if (auto logic = std::get_if<LogicResult>(&result)) {
    return logic->size();
  }
  return std::get<bool>(result) ? 1 : 0;
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

  auto orResult = runBench("boolean_or_assign_64", 5000,
                           [&] { return branchCount(wideOr(ctx)); });
  EXPECT_EQ(64ull * orResult.iterations, orResult.checksum);

  auto andResult = runBench("boolean_and_cross_16x16x16", 300,
                            [&] { return branchCount(wideAnd(ctx)); });
  EXPECT_EQ(4096ull * andResult.iterations, andResult.checksum);
}

}  // namespace boolean_perf
