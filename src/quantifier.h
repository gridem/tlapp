#pragma once

#include "boolean.h"

namespace detail {

fun(quantifierBoolOp, onEmpty, termination, ctx, setExpr, elem, predicateExpr) {
  bool resultOnEmpty = onEmpty;
  bool resultOnTermination = termination;
  auto&& set = extract(fwd(ctx, setExpr));

  if (set.empty()) {
    return resultOnEmpty;
  }
  for (auto&& e : set) {
    *elem = &e;
    auto result = extract(fwd(ctx, predicateExpr));
    if (result == resultOnTermination) {
      return resultOnTermination;
    }
  }
  return !resultOnTermination;
}

fun(existsBooleanOp, ctx, setExpr, elem, predicateExpr) {
  auto&& set = extract(fwd(ctx, setExpr));
  if (set.empty()) {
    return BooleanResult{false};
  }

  LogicResult ors;
  if constexpr (requires { set.size(); }) {
    ors.reserve(set.size());
  }
  for (auto&& e : set) {
    *elem = &e;
    auto result = extract(fwd(ctx, predicateExpr));
    if (auto bPtr = std::get_if<bool>(&result)) {
      if (*bPtr) {
        return BooleanResult{true};
      }
      continue;
    }
    auto&& logic = std::get<LogicResult>(result);
    appendLogic(ors, logic);
  }
  if (ors.empty()) {
    return BooleanResult{false};
  }
  return BooleanResult{std::move(ors)};
}

fun(forallBooleanOp, ctx, setExpr, elem, predicateExpr) {
  auto&& set = extract(fwd(ctx, setExpr));
  if (set.empty()) {
    return BooleanResult{true};
  }

  LogicResult ands;
  bool hasLogic = false;
  for (auto&& e : set) {
    *elem = &e;
    auto result = extract(fwd(ctx, predicateExpr));
    if (auto bPtr = std::get_if<bool>(&result)) {
      if (!*bPtr) {
        return BooleanResult{false};
      }
      continue;
    }
    if (!hasLogic) {
      ands = std::move(std::get<LogicResult>(result));
      hasLogic = true;
    } else {
      ands = mulVectors(std::move(ands), std::get<LogicResult>(result));
    }
  }
  if (hasLogic) {
    return BooleanResult{std::move(ands)};
  }
  return BooleanResult{true};
}

fun(forallOp, ctx, setExpr, elem, predicateExpr) {
  if constexpr (is_eq<ExpressionType<T_predicateExpr>, bool>) {
    return quantifierBoolOp(true /* onEmpty */, false /* termination */,
                            fwd(ctx, setExpr, elem, predicateExpr));
  } else {
    return forallBooleanOp(fwd(ctx, setExpr, elem, predicateExpr));
  }
}

fun(existsOp, ctx, setExpr, elem, predicateExpr) {
  if constexpr (is_eq<ExpressionType<T_predicateExpr>, bool>) {
    return quantifierBoolOp(false /* onEmpty */, true /* termination */,
                            fwd(ctx, setExpr, elem, predicateExpr));
  } else {
    return existsBooleanOp(fwd(ctx, setExpr, elem, predicateExpr));
  }
}

// quantifierOp is operation for quantifier.
// Predicate is a function of element of the set, where element is expression.
fun(quantifier, quantifierOp, set, predicate) {
  /*
  steps:
  1. extract type from set:
    can be immediate or expression.
  2. Create shared ptr instance of set value type.
  3. Invoke predicate with expression containing shared_ptr instance.
  4. Return expression
  */
  auto elem =
      std::make_shared<const typename OperandType<T_set>::value_type*>(nullptr);
  auto predicateExpr =
      predicate(finalize([ptr = elem.get()] lam_arg(ctx) { return **ptr; }));
  return evaluate_ctx(fwd(quantifierOp, set), std::move(elem),
                      std::move(predicateExpr));
}

}  // namespace detail

fun(forall, set, predicate) {
  return detail::quantifier(as_lam(detail::forallOp), fwd(set, predicate));
}

// Predicate is a function of element of the set, where element is expression.
fun(exists, set, predicate) {
  return detail::quantifier(as_lam(detail::existsOp), fwd(set, predicate));
}

namespace detail {

tname(T, F) struct MacroBlock : F {
  tname(T1, F1) MacroBlock(T1&& t, F1&& f) : F{fwd(f)}, t_{fwd(t)} {}

  fun(operator^, f) { return F::operator()(std::move(t_), fwd(f)); }

 private:
  T t_;
};

fun(macroBlock, in, f) {
  return MacroBlock<std::decay_t<T_in>, std::decay_t<T_f>>{fwd(in, f)};
}

}  // namespace detail

#define macro_block(D_var, D_in, D_fun) \
  detail::macroBlock(detail::prepare(D_in), as_lam(D_fun)) ^ [&] lam_arg(D_var)

#define $A(D_var, D_in) macro_block(D_var, D_in, forall)
#define $E(D_var, D_in) macro_block(D_var, D_in, exists)
