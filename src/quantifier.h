#pragma once

#include "boolean.h"

namespace detail {

fun(earlyTerminationCheck, result, termination) {
  if_eq(result, bool) {
    if (result == termination) {
      // Early termination without iterating through all items.
      return true;
    }
  }
  else if_eq(result, BooleanResult) {
    if (auto bPtr = std::get_if<bool>(&result)) {
      if (*bPtr == termination) {
        // Early termination without iterating through all items for
        // BooleanResult.
        return true;
      }
    }
  }
  return false;
}

// Generic quantifier operation.
// It seems complex, but the sequence is simple:
//    1. Check on empty set. Do early return if it's empty.
//    2. Iterate through set values.
//    3. During iteration we need to setup element since we have complex
//       shared_ptr passing lam_arg through the pointer: *elem = *begin.
//    4. Iteration is manual since we need to assign the type and do
//       optimization for the first element. If it's LogicResult, a raw bool
//       is not acceptable here.
//    5. Do early termination if we know the final result without checking the
//       rest.
//    6. Apply quantifier operation (|| or &&) iteratively.
// Manual first-element handling avoids collapsing a LogicResult into bool.
fun(quantifierOp, onEmpty, termination, op, ctx, setExpr, elem, predicateExpr) {
  auto set = extract(fwd(ctx, setExpr));

  using ResultType =
      BinaryBooleanResultType<bool, ExpressionType<T_predicateExpr>>;

  if (set.empty()) {
    return ResultType{onEmpty};
  }
  auto it = std::begin(set);
  auto end = std::end(set);
  *elem = &*it;
  auto result = extract(fwd(ctx, predicateExpr));
  for (++it; it != end; ++it) {
    if (earlyTerminationCheck(result, termination)) {
      return ResultType{termination};
    }
    *elem = &*it;
    result = op(std::move(result), extract(fwd(ctx, predicateExpr)));
  }
  return ResultType{result};
}

funs(forallOp, v) {
  return quantifierOp(true /* onEmpty */, false /* termination */,
                      as_lam(andOp), fwd(v)...);
}

funs(existsOp, v) {
  return quantifierOp(false /* onEmpty */, true /* termination */, as_lam(orOp),
                      fwd(v)...);
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
