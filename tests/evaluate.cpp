#include "evaluate.h"

#include <gtest/gtest.h>

#include "var.h"

namespace test {

TEST(Evaluate, Inc) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;
  ASSERT_EQ(2, i(ctx));

  auto i1 = evaluate(lam_in(v, v + 1), i);
  ASSERT_EQ(3, i1(ctx));
}

TEST(Evaluate, Const) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;
  ASSERT_EQ(2, i(ctx));

  auto i1 = evaluate(lam_in(a, b, a + b), i, 4);
  ASSERT_EQ(6, i1(ctx));
}

TEST(Evaluate, NestExpression) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;
  ASSERT_EQ(2, i(ctx));

  auto inc = lam_in(v, v + 1);

  auto i1 = evaluate(inc, i);
  auto i2 = evaluate(inc, i1);
  ASSERT_EQ(4, i2(ctx));
}

TEST(Evaluate, WithCtx) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;
  ASSERT_EQ(2, i(ctx));

  auto inc = lam(ctx, v) {
    if (ctx.isInit()) {
      throw 1;
    }
    return v(ctx) + 1;
  };

  EXPECT_THROW(evaluate_ctx(inc, i)(ctx), int);
  ctx.setState(LogicState::Next);
  EXPECT_EQ(3, evaluate_ctx(inc, i)(ctx));
}

TEST(Evaluate, Lazy) {
  Var<int> i{"i"};

  auto e = evaluate_lazy(lam_in(a, b, a() + b()), i, 3);

  Context ctx;
  i.getRef(ctx) = 2;

  EXPECT_EQ(5, e(ctx));
}

TEST(Evaluate, EvaluatorLazy) {
  Var<int> i{"i"};
  int x = 5;

  auto e = evaluator_lazy(x() - i(), x, i);

  Context ctx;
  i.getRef(ctx) = 2;

  EXPECT_EQ(3, e(ctx));
}

}  // namespace test
