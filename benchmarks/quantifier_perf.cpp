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

  expectBenchChecksum("quant_forall_late_fail_4096", 2000, 0,
                      [&] { return forallLateFail(ctx); });

  expectBenchPerIteration("quant_exists_early_hit_4096", 5000, 1,
                          [&] { return existsEarlyHit(ctx); });

  expectBenchPerIteration("quant_exists_late_hit_4096", 2000, 1,
                          [&] { return existsLateHit(ctx); });

  expectBenchPerIteration("quant_exists_assign_16", 5000, 16,
                          [&] { return existsAssign(ctx); });
}

}  // namespace quantifier_perf
