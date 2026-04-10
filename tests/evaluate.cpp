#include "evaluate.h"

#include <gtest/gtest.h>

#include "boolean.h"
#include "operation.h"
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

TEST(Evaluate, BindPredicateModeUnwrapsBooleanResult) {
  Var<int> i{"i"};

  auto expr = i == 2;
  auto bound = bind(expr, PredicateMode{});
  static_assert(is_eq<BoundPredicate<decltype(expr)>, decltype(bound)>);
  static_assert(is_eq<bool, decltype(bound(std::declval<Context&>()))>);

  Context ctx;
  i.getRef(ctx) = 2;
  EXPECT_TRUE(bound(ctx));

  i.getRef(ctx) = 3;
  EXPECT_FALSE(bound(ctx));
}

TEST(Evaluate, BindPredicateModePreservesPredicateSemantics) {
  Var<int> i{"i"};

  auto expr = i > 2;
  auto bound = bind(expr, PredicateMode{});
  static_assert(is_eq<BoundPredicate<decltype(expr)>, decltype(bound)>);

  Context ctx;
  i.getRef(ctx) = 1;
  EXPECT_FALSE(bound(ctx));

  i.getRef(ctx) = 3;
  EXPECT_TRUE(bound(ctx));
}

TEST(Evaluate, BindPredicateModeDoesNotForceCheckForBoolPredicate) {
  auto expr = Expression<bool>{[](Context& ctx) { return ctx.isCheck() ? false : true; }};
  auto bound = bind(expr, PredicateMode{});

  Context ctx;
  EXPECT_TRUE(bound(ctx));
  EXPECT_FALSE(ctx.isCheck());
}

TEST(Evaluate, BindPredicateModeRejectsBranchProducingResult) {
  auto expr = Boolean{
      [](Context&) { return LogicResult::fromRaw([](Context&) { return true; }); }};
  auto bound = bind(expr, PredicateMode{});

  Context ctx;
  ASSERT_FALSE(ctx.isCheck());
  EXPECT_THROW(bound(ctx), EngineBooleanError);
  EXPECT_FALSE(ctx.isCheck());
}

TEST(Evaluate, BindPredicateModeRestoresCheckFlagAfterThrow) {
  auto expr = Boolean{[](Context& ctx) -> BooleanResult {
    EXPECT_TRUE(ctx.isCheck());
    throw 1;
  }};
  auto bound = bind(expr, PredicateMode{});

  Context ctx;
  ASSERT_FALSE(ctx.isCheck());
  EXPECT_THROW(bound(ctx), int);
  EXPECT_FALSE(ctx.isCheck());
}

TEST(Evaluate, BindInitModePreservesAssignmentSemantics) {
  Var<int> i{"i"};

  auto expr = i == 2;
  auto bound = bind(expr, InitMode{});
  static_assert(is_eq<BoundInitAction<decltype(expr)>, decltype(bound)>);
  static_assert(is_eq<BooleanResult, decltype(bound(std::declval<Context&>()))>);

  Context ctx;
  auto res = bound(ctx);
  auto&& ors = std::get<LogicResult>(res);

  ASSERT_EQ(1, ors.size());
  ASSERT_TRUE(ors[0](ctx));
  EXPECT_EQ(2, i(ctx));
}

TEST(Evaluate, BindNextModePreservesAssignmentSemantics) {
  Var<int> i{"i"};

  auto expr = i++ == 3;
  auto bound = bind(expr, NextMode{});
  static_assert(is_eq<BoundNextAction<decltype(expr)>, decltype(bound)>);
  static_assert(is_eq<BooleanResult, decltype(bound(std::declval<Context&>()))>);

  Context ctx;
  i.getRef(ctx) = 1;
  ctx.setState(LogicState::Next);

  auto res = bound(ctx);
  auto&& ors = std::get<LogicResult>(res);

  ASSERT_EQ(1, ors.size());
  ASSERT_TRUE(ors[0](ctx));
  EXPECT_EQ(3, i++(ctx));
}

}  // namespace test
