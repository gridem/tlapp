#include "boolean.h"

#include <gtest/gtest.h>

#include "operation.h"
#include "troubleshooting.h"
#include "var.h"

namespace test {

TEST(Boolean, FromRaw) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;

  auto e = i > 2;
  auto lr = LogicResult::fromRaw(fwd(e));

  ASSERT_FALSE(lr.evaluate(ctx));

  auto br = BooleanResult::fromRaw(fwd(e));

  ASSERT_ANY_THROW(std::get<bool>(br));
  ASSERT_FALSE(std::get<LogicResult>(br).evaluate(ctx));
}

TEST(Boolean, ToConst) {
  Var<int> i{"i"};

  Context ctx;
  auto e = i == 2;
  Boolean be = e;

  // No initialized variable should be there.
  EXPECT_EQ(0, ctx.size());
  auto v = e(ctx);
  EXPECT_EQ(0, ctx.size());
  ASSERT_THROW(i(ctx), VarInitError);

  // After execution the variable must be initialized.
  ASSERT_TRUE(std::get<LogicResult>(v).evaluate(ctx));
  EXPECT_EQ(1, ctx.size());
  ASSERT_EQ(2, i(ctx));
}

TEST(Boolean, TypeCheck) {
  Var<int> i{"i"};

  Context ctx;
  auto e1 = i > 2;
  auto e2 = i == 2;

  static_assert(is_eq<bool, decltype(e1(ctx))>);
  static_assert(is_eq<BooleanResult, decltype(e2(ctx))>);

  static_assert(is_eq<bool, decltype((e1 || e1)(ctx))>);
  static_assert(is_eq<bool, decltype((e1 && e1)(ctx))>);

  static_assert(is_eq<BooleanResult, decltype((e1 || e2)(ctx))>);
  static_assert(is_eq<BooleanResult, decltype((e2 || e1)(ctx))>);
  static_assert(is_eq<BooleanResult, decltype((e2 || e2)(ctx))>);

  static_assert(is_eq<BooleanResult, decltype((e1 && e2)(ctx))>);
  static_assert(is_eq<BooleanResult, decltype((e2 && e1)(ctx))>);
  static_assert(is_eq<BooleanResult, decltype((e2 && e2)(ctx))>);
}

TEST(Boolean, And) {
  Var<int> x{"x"};
  Context ctx;

  {
    auto expr = x > 1 && true;

    x.getRef(ctx) = 1;
    EXPECT_FALSE(expr(ctx));

    x.getRef(ctx) = 2;
    EXPECT_TRUE(expr(ctx));
  }

  {
    auto expr = x > 1 && false;

    x.getRef(ctx) = 1;
    EXPECT_FALSE(expr(ctx));

    x.getRef(ctx) = 2;
    EXPECT_FALSE(expr(ctx));
  }
}

TEST(Boolean, Or) {
  Var<int> x{"x"};
  Context ctx;

  {
    auto expr = x > 1 || true;

    x.getRef(ctx) = 1;
    EXPECT_TRUE(expr(ctx));

    x.getRef(ctx) = 2;
    EXPECT_TRUE(expr(ctx));
  }

  {
    auto expr = x > 1 || false;

    x.getRef(ctx) = 1;
    EXPECT_FALSE(expr(ctx));

    x.getRef(ctx) = 2;
    EXPECT_TRUE(expr(ctx));
  }
}

TEST(Boolean, OpOr) {
  Var<int> x{"x"};
  Var<int> y{"y"};

  auto e = x++ == 2 && y++ == 3 && x > 0 || x++ == 3 && y > 3 && x < 2;

  Context ctx;
  {
    x.getRef(ctx) = 1;
    y.getRef(ctx) = 4;

    ctx.setState(LogicState::Next);
    auto res = e(ctx);
    auto&& ors = std::get<LogicResult>(res);

    ASSERT_EQ(2, ors.size());
    ASSERT_EQ(1, ors[0].size());
    ASSERT_EQ(1, ors[1].size());

    {
      ASSERT_TRUE(ors[0](ctx));
      EXPECT_EQ(2, x++(ctx));
      EXPECT_EQ(3, y++(ctx));
    }
    { ASSERT_FALSE(ors[1](ctx)); }
  }

  {
    x.getRef(ctx) = 1;
    y.getRef(ctx) = 1;

    ctx.setState(LogicState::Next);
    auto res = e(ctx);
    auto&& ors = std::get<LogicResult>(res);

    ASSERT_EQ(1, ors.size());
    ASSERT_EQ(1, ors[0].size());
  }

  {
    x.getRef(ctx) = 0;
    y.getRef(ctx) = 1;

    ctx.setState(LogicState::Next);
    auto res = e(ctx);
    ASSERT_FALSE(std::get<bool>(res));
  }
}

TEST(Boolean, Not) {
  Var<int> x{"x"};

  auto e = !(x > 2);

  Context ctx;
  ASSERT_ANY_THROW(e(ctx));

  x.getRef(ctx) = 1;
  ASSERT_TRUE(e(ctx));

  x.getRef(ctx) = 2;
  ASSERT_TRUE(e(ctx));

  x.getRef(ctx) = 3;
  ASSERT_FALSE(e(ctx));
}

TEST(Expression, NotOr) {
  Var<int> x{"x"};
  Var<int> y{"y"};

  auto e = !(x < 2 || y >= 2);

  Context ctx;
  ASSERT_ANY_THROW(e(ctx));

  auto check = [&](int setX, int setY, bool expected) {
    x.getRef(ctx) = setX;
    y.getRef(ctx) = setY;
    ASSERT_EQ(expected, e(ctx));
  };

  check(1, 1, false);
  check(1, 2, false);
  check(2, 1, true);
  check(2, 2, false);
}

TEST(Expression, NotAnd) {
  Var<int> x{"x"};
  Var<int> y{"y"};

  auto e = !(x < 2 && y >= 2);

  Context ctx;
  ASSERT_ANY_THROW(e(ctx));

  auto check = [&](int setX, int setY, bool expected) {
    x.getRef(ctx) = setX;
    y.getRef(ctx) = setY;
    ASSERT_EQ(expected, e(ctx));
  };

  check(1, 1, true);
  check(1, 2, false);
  check(2, 1, true);
  check(2, 2, true);
}

}  // namespace test
