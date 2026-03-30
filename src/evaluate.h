#pragma once

#include "error.h"
#include "tag.h"

namespace detail {

// Prepares expression
fun(prepare, x) {
  if_is(x, var) { return x.toExpression(); }
  else {
    return fwd(x);
  }
}

// Extracts value from expression or other types like const.
fun(extract, ctx, x) {
  if_is(x, immediate) { return fwd(x); }
  else {
    return x(ctx);
  }
}

// Converts to appropriate type at the end of expression.
fun(finalize, f) { return fwd(f) & expression_tag; }

}  // namespace detail

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
  return detail::finalize(
      [capture(f), ... e = detail::prepare(fwd(v))] lam_arg(ctx) {
        return f([&e, &ctx] { return detail::extract(ctx, e); } & lazy_tag...);
      });
}

// Helper macro to quickly evaluate expressions.
// Sample: evaluator_lazy(l() + r(), l, r) returns sum of l and r.
#define evaluator_lazy(f, ...) \
  evaluate_lazy(lam_in(__VA_ARGS__, f), fwd(__VA_ARGS__))

// Helper macro to quickly evaluate expressions using functions.
// Sample: evaluator_lazy_fun(myOp, l, r) returns myOp(l(), r()).
#define evaluator_lazy_fun(f, ...) evaluate_lazy(as_lam(f), fwd(__VA_ARGS__))
