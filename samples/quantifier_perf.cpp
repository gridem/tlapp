#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "bench_util.h"
#include "operation.h"
#include "quantifier.h"
#include "var.h"

namespace quantifier_perf {
namespace {

std::vector<int> makeRange(int size) {
  std::vector<int> values(size);
  std::iota(values.begin(), values.end(), 0);
  return values;
}

std::vector<int> makeSmallRange(int size) {
  std::vector<int> values(size);
  std::iota(values.begin(), values.end(), 1);
  return values;
}

uint64_t branchCount(const BooleanResult& result) {
  if (auto logic = std::get_if<LogicResult>(&result)) {
    return logic->size();
  }
  return std::get<bool>(result) ? 1 : 0;
}

}  // namespace

TEST(QuantifierPerf, Run) {
  Var<std::vector<int>> vec{"vec"};
  Var<int> x{"x"};

  auto values = makeRange(4096);
  auto smallValues = makeSmallRange(16);

  auto forallLateFail = forall(vec, lam_in(i, i != 4095));
  auto existsEarlyHit = exists(vec, lam_in(i, i == 0));
  auto existsLateHit = exists(vec, lam_in(i, i == 4095));
  auto existsAssign =
      exists(smallValues, [&] lam_arg(i) { return x == i; });

  Context ctx;
  vec.getRef(ctx) = values;

  auto forallResult = runBench("quant_forall_late_fail_4096", 2000,
                               [&] { return static_cast<uint64_t>(forallLateFail(ctx)); });
  EXPECT_EQ(0ull, forallResult.checksum);

  auto existsEarlyResult = runBench("quant_exists_early_hit_4096", 5000,
                                    [&] { return static_cast<uint64_t>(existsEarlyHit(ctx)); });
  EXPECT_EQ(existsEarlyResult.iterations, existsEarlyResult.checksum);

  auto existsLateResult = runBench("quant_exists_late_hit_4096", 2000,
                                   [&] { return static_cast<uint64_t>(existsLateHit(ctx)); });
  EXPECT_EQ(existsLateResult.iterations, existsLateResult.checksum);

  auto assignResult = runBench("quant_exists_assign_16", 5000,
                               [&] { return branchCount(existsAssign(ctx)); });
  EXPECT_EQ(16ull * assignResult.iterations, assignResult.checksum);
}

}  // namespace quantifier_perf
