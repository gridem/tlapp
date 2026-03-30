#include "quantifier.h"

#include <gtest/gtest.h>

#include "operation.h"
#include "var.h"

namespace test {

TEST(Quantifier, Forall) {
  Var<std::vector<int>> vec{"vec"};

  auto e = forall(vec, lam_in(i, i != 2));

  Context ctx;

  auto check = [&](bool expected, std::vector<int> v) {
    vec.getRef(ctx) = std::move(v);
    EXPECT_EQ(expected, e(ctx));
  };

  check(true, {1});
  check(false, {1, 2});
  check(false, {2, 1});
  check(true, {1, 3, 4});
  check(false, {1, 3, 2});
}

TEST(Quantifier, ForallConst) {
  Var<int> x{"x"};

  auto e = forall(std::vector<int>{1, 2}, [&] lam_arg(i) { return x != i; });

  Context ctx;

  auto check = [&](bool expected, int v) {
    x.getRef(ctx) = v;
    EXPECT_EQ(expected, e(ctx));
  };

  check(true, 0);
  check(false, 1);
  check(false, 2);
  check(true, 3);
}

TEST(Quantifier, ForallExpr) {
  Var<std::vector<int>> vec{"vec"};
  Var<int> x{"x"};

  auto e = forall(vec, [&] lam_arg(i) { return x != i; });

  Context ctx;

  auto check = [&](bool expected, std::vector<int> vs, int v) {
    vec.getRef(ctx) = std::move(vs);
    x.getRef(ctx) = v;
    EXPECT_EQ(expected, e(ctx));
  };

  check(true, {1, 2}, 0);
  check(false, {1, 2}, 1);
  check(false, {1, 2}, 2);
  check(true, {1, 2}, 3);

  check(false, {1, 2}, 2);
  check(false, {2, 1}, 2);
  check(true, {1, 3, 4}, 2);
  check(false, {1, 3, 2}, 2);
}

TEST(Quantifier, Exists) {
  Var<std::vector<int>> vec{"vec"};

  auto e = exists(vec, lam_in(i, i == 2));

  Context ctx;
  ctx.setCheck(true);

  auto check = [&](bool expected, std::vector<int> v) {
    auto vStr = asString(v);
    vec.getRef(ctx) = std::move(v);
    EXPECT_EQ(expected, e(ctx)) << "for set: " << vStr;
  };

  check(false, {1});
  check(true, {1, 2});
  check(true, {2, 1});
  check(false, {1, 3, 4});
  check(true, {1, 3, 2});
}

TEST(Quantifier, ExistsConst) {
  Var<int> x{"x"};

  auto e = exists(std::vector<int>{1, 2}, [&] lam_arg(i) { return x == i; });

  Context ctx;
  ctx.setCheck(true);

  auto check = [&](bool expected, int v) {
    x.getRef(ctx) = v;
    auto res = e(ctx);
    EXPECT_EQ(expected, std::get<bool>(e(ctx)));
  };

  check(!true, 0);
  check(!false, 1);
  check(!false, 2);
  check(!true, 3);
}

TEST(Quantifier, ExistsExpr) {
  Var<std::vector<int>> vec{"vec"};
  Var<int> x{"x"};

  auto e = exists(vec, [&] lam_arg(i) { return x == i; });

  Context ctx;
  ctx.setCheck(true);

  auto check = [&](bool expected, std::vector<int> vs, int v) {
    vec.getRef(ctx) = std::move(vs);
    x.getRef(ctx) = v;
    EXPECT_EQ(expected, std::get<bool>(e(ctx)));
  };

  check(!true, {1, 2}, 0);
  check(!false, {1, 2}, 1);
  check(!false, {1, 2}, 2);
  check(!true, {1, 2}, 3);

  check(!false, {1, 2}, 2);
  check(!false, {2, 1}, 2);
  check(!true, {1, 3, 4}, 2);
  check(!false, {1, 3, 2}, 2);
}

TEST(Quantifier, ForallMacro) {
  Var<std::vector<int>> vec{"vec"};
  Var<int> x{"x"};
  auto e = $A(i, vec) { return i != x; };

  Context ctx;

  auto check = [&](bool expected, std::vector<int> vs, int v) {
    vec.getRef(ctx) = std::move(vs);
    x.getRef(ctx) = v;
    EXPECT_EQ(expected, e(ctx));
  };

  check(true, {1, 2}, 0);
  check(false, {1, 2}, 1);
  check(false, {1, 2}, 2);
  check(true, {1, 2}, 3);

  check(false, {1, 2}, 2);
  check(false, {2, 1}, 2);
  check(true, {1, 3, 4}, 2);
  check(false, {1, 3, 2}, 2);
}

TEST(Quantifier, ForallMacroRVal) {
  Var<int> x{"x"};
  auto e = $A(i, (std::vector<int>{1, 2})) { return i != x; };

  Context ctx;

  auto check = [&](int vx, bool expected) {
    x.getRef(ctx) = vx;
    ASSERT_EQ(expected, e(ctx));
  };
  check(1, false);
  check(2, false);
  check(0, true);
  check(3, true);
}

TEST(Quantifier, ExistsMacro) {
  Var<std::vector<int>> vec{"vec"};
  Var<int> x{"x"};

  auto e = $E(i, vec) { return i == x; };

  Context ctx;
  ctx.setCheck(true);

  auto check = [&](bool expected, std::vector<int> vs, int v) {
    vec.getRef(ctx) = std::move(vs);
    x.getRef(ctx) = v;
    ASSERT_EQ(expected, e(ctx));
  };

  check(!true, {1, 2}, 0);
  check(!false, {1, 2}, 1);
  check(!false, {1, 2}, 2);
  check(!true, {1, 2}, 3);

  check(!false, {1, 2}, 2);
  check(!false, {2, 1}, 2);
  check(!true, {1, 3, 4}, 2);
  check(!false, {1, 3, 2}, 2);
}

}  // namespace test
