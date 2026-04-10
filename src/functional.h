#pragma once

#include "boolean.h"
#include "evaluate.h"
#include "macro.h"
#include "quantifier.h"

// TODO: refactor quantifier to generalize logic for both filter & quantifier.

funs(unchanged, var, us) {
  if constexpr (sizeof...(us) == 0) {
    return var++ == var;
  } else {
    return var++ == var && unchanged(fwd(us)...);
  }
}

namespace detail {

fun(filterContainerOp, container, predicate) {
  std::decay_t<decltype(container)> result;
  std::copy_if(container.begin(), container.end(), std::inserter(result, result.end()),
      fwd(predicate));
  return result;
};

/*
// Converts a lambda predicate to an expression predicate.
// Note: this block is currently unused and may be removed or updated
// alongside the quantifier refactor.
fun(lamPredicateToExpressionPredicate, predicate) {
  auto elem =
      std::make_shared<typename OperandType<T_set>::value_type*>(nullptr);
  auto expressionPredicate =
      predicate(finalize([ptr = elem.get()] lam_arg(ctx) { return **ptr; }));
  return [capture_move(elem, expressionPredicate)] lam_arg(ctx, e) {
    *elem = &e;
    return expressionPredicate(ctx);
  };
}
*/

fun(filterOp, ctx, setExpr, elem, predicateExpr) {
  auto&& set = extract(fwd(ctx, setExpr));

  std::decay_t<decltype(set)> result;
  auto inserter = std::inserter(result, result.end());

  for (auto&& e : set) {
    *elem = &e;
    auto predicateResult = extract(fwd(ctx, predicateExpr));
    if (predicateResult) {
      *inserter++ = e;
    }
  }
  return result;
}

}  // namespace detail

fun(filter, container, predicate) {
  return detail::quantifier(as_lam(detail::filterOp), fwd(container, predicate));
}

#define $if(D_var, D_in) macro_block(D_var, D_in, filter)
