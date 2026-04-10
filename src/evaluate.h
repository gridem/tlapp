#pragma once

#include <utility>

#include "context.h"
#include "error.h"
#include "expression.h"
#include "tag.h"

namespace detail {

// Prepares expression
fun(prepare, x) {
  if_is(x, var) {
    return x.toExpression();
  } else {
    return fwd(x);
  }
}

// Extracts value from expression or other types like const.
fun(extract, ctx, x) {
  if_is(x, immediate) {
    return fwd(x);
  } else {
    return x(ctx);
  }
}

// Converts to appropriate type at the end of expression.
fun(finalize, f) {
  return fwd(f) & expression_tag;
}

}  // namespace detail

struct PredicateMode {};

struct InitMode {};

struct NextMode {};

namespace detail {

struct CheckModeGuard {
  explicit CheckModeGuard(Context& ctx) : ctx_{ctx}, wasCheck_{ctx.isCheck()} {
    ctx_.setCheck(true);
  }

  ~CheckModeGuard() {
    ctx_.setCheck(wasCheck_);
  }

 private:
  Context& ctx_;
  bool wasCheck_;
};

template <typename T>
struct PredicateResult {
  static bool apply(const T&) = delete;
};

template <typename T>
struct PredicateNeedsCheck : std::false_type {};

fun(bindEval, ctx, expr, mode) {
  if_eq(mode, PredicateMode) {
    if constexpr (!is_immediate<T_expr> &&
                  PredicateNeedsCheck<OperandType<T_expr>>::value) {
      CheckModeGuard guard{ctx};
      auto result = detail::extract(ctx, expr);
      return PredicateResult<std::decay_t<decltype(result)>>::apply(result);
    }
    auto result = detail::extract(fwd(ctx), expr);
    return PredicateResult<std::decay_t<decltype(result)>>::apply(result);
  } else {
    return detail::extract(fwd(ctx), expr);
  }
}

}  // namespace detail

tname(T_expr) using PreparedExpression =
    std::decay_t<decltype(detail::prepare(std::declval<T_expr>()))>;

tname(T_expr, T_mode) using BoundResult =
    std::decay_t<decltype(detail::bindEval(std::declval<Context&>(),
        std::declval<PreparedExpression<T_expr>&>(),
        std::declval<T_mode>()))>;

tname(T_expr, T_mode) using BoundExpression = Expression<BoundResult<T_expr, T_mode>>;

tname(T_expr) using BoundPredicate = BoundExpression<T_expr, PredicateMode>;

tname(T_expr) using BoundInitAction = BoundExpression<T_expr, InitMode>;

tname(T_expr) using BoundNextAction = BoundExpression<T_expr, NextMode>;

fun(bind, expr, mode) {
  using Mode = std::decay_t<T_mode>;
  using BoundExpr = BoundExpression<T_expr, Mode>;
  auto prepared = detail::prepare(fwd(expr));
  return BoundExpr{[capture(prepared)] lam_arg(
                       ctx) { return detail::bindEval(ctx, prepared, Mode{}); }};
}

// Extracts the expression values and invokes the corresponding lambda.
funs(evaluate, f, v) {
  static_assert(!(is_immediate<T_v> && ...));
  return detail::finalize([capture(f), ... e = detail::prepare(fwd(v))] lam_arg(
                              ctx) { return f(detail::extract(ctx, e)...); });
}

// Helper macro to quickly evaluate expressions.
// Sample: evaluator(l + r, l, r) returns sum of l and r.
#define evaluator(f, ...) evaluate(lam_in(__VA_ARGS__, f), fwd(__VA_ARGS__))

// Helper macro to quickly evaluate expressions using functions.
// Sample: evaluator_fun(myOp, l, r) returns myOp(l, r).
#define evaluator_fun(f, ...) evaluate(as_lam(f), fwd(__VA_ARGS__))

// Invokes the corresponding lambda with context as first argument without
// values extraction.
funs(evaluate_ctx, f, v) {
  static_assert(!(is_immediate<T_v> && ...));
  return detail::finalize([capture(f), ... e = detail::prepare(fwd(v))] lam_arg(
                              ctx) { return f(ctx, e...); });
}

// Extracts the expression values and invokes the corresponding lambda with lazy
// evaluation.
funs(evaluate_lazy, f, v) {
  static_assert(!(is_immediate<T_v> && ...));
  return detail::finalize([capture(f), ... e = detail::prepare(fwd(v))] lam_arg(ctx) {
    return f([&e, &ctx] { return detail::extract(ctx, e); } & lazy_tag...);
  });
}

// Helper macro to quickly evaluate expressions.
// Sample: evaluator_lazy(l() + r(), l, r) returns sum of l and r.
#define evaluator_lazy(f, ...) evaluate_lazy(lam_in(__VA_ARGS__, f), fwd(__VA_ARGS__))

// Helper macro to quickly evaluate expressions using functions.
// Sample: evaluator_lazy_fun(myOp, l, r) returns myOp(l(), r()).
#define evaluator_lazy_fun(f, ...) evaluate_lazy(as_lam(f), fwd(__VA_ARGS__))
