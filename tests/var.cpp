#include "var.h"

#include <gtest/gtest.h>

namespace test {

TEST(Var, Init) {
  Var<int> i{"i"};

  Context ctx;
  ASSERT_NO_THROW(i.getRef(ctx));
}

TEST(Var, GetNonEmpty) {
  Var<int> i{"i"};

  auto x = i.toExpression();

  Context ctx;

  ASSERT_THROW(x(ctx), VarInitError);

  i.getRef(ctx) = 2;
  auto res = x(ctx);
  ASSERT_EQ(res, 2);
}

TEST(Var, Assign) {
  Var<int> i{"i"};

  auto x = i.toExpression();

  Context ctx;

  ASSERT_TRUE(x.assignTo(ctx, 2));

  ASSERT_EQ(2, i.getRef(ctx));
}

TEST(Var, AssignTo) {
  Var<int> i{"i"};

  auto x = i.toExpression();
  auto n = i++;

  Context ctx;

  ASSERT_THROW(n(ctx), VarInitError);

  ASSERT_THROW(n.assignTo(ctx, 1), VarInitError);

  ASSERT_TRUE(x.assignTo(ctx, 2));

  ASSERT_THROW(n.assignTo(ctx, 3), VarInitError);
  ctx.setState(LogicState::Next);
  ASSERT_TRUE(n.assignTo(ctx, 3));

  ASSERT_EQ(2, i.getRef(ctx));
  ASSERT_EQ(3, i++.getRef(ctx));

  ASSERT_TRUE(x.assignTo(ctx, 2));
  ASSERT_FALSE(x.assignTo(ctx, 3));

  ASSERT_TRUE(n.assignTo(ctx, 3));
  ASSERT_FALSE(n.assignTo(ctx, 2));
}

}  // namespace test
