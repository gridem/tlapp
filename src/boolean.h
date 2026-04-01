#pragma once

#include "evaluate.h"
#include "expression.h"

namespace detail {

// Specify raw boolean expression.
using RawBoolean = RawX<bool>;

}  // namespace detail

// Set of assigns.
// Represents a list of booleans with 'and' operation on them.
struct AssignsResult : detail::RawBoolean {
  using detail::RawBoolean::RawBoolean;

  // Compatibility mode.
  size_t size() const { return 1; }
};

// Set of set of assigns.
// Represents a list of booleans with 'or' operation on set of assigns.
struct LogicResult : std::vector<AssignsResult> {
  using std::vector<AssignsResult>::vector;

  tname(B) static LogicResult fromRaw(B&& b) { return {fwd(b)}; }

  // Should be used for testing purposes only because it represents a set of
  // possible combinations.
  bool evaluate(Context& ctx) const {
    for (auto&& o : *this) {
      if (o(ctx) == true) {
        return true;
      }
    }
    return false;
  }
};

// Represents generic boolean result.
// It could be raw boolean,
// or matrix of possible combinations with assignments.
struct BooleanResult : std::variant<bool, LogicResult> {
  using std::variant<bool, LogicResult>::variant;

  tname(B) static BooleanResult fromRaw(B&& b) {
    return LogicResult::fromRaw(fwd(b));
  }

  // Should be used for testing purposes only because it represents a set of
  // possible combinations.
  bool evaluate(Context& ctx) const {
    return std::visit(
        [&ctx] lam_arg(v) {
          if_eq(v, bool) { return v; }
          else {
            return v.evaluate(ctx);
          }
        },
        *this);
  }
};

using Boolean = Expression<BooleanResult>;

namespace detail {

template <>
struct PredicateResult<bool> {
  static bool apply(bool result) { return result; }
};

template <>
struct PredicateNeedsCheck<BooleanResult> : std::true_type {};

template <>
struct PredicateResult<BooleanResult> {
  static bool apply(const BooleanResult& result) {
    if (auto b = std::get_if<bool>(&result)) {
      return *b;
    }
    throw EngineBooleanError("Invalid boolean value type: must be simple bool");
  }
};

fun(isAssign, var, ctx) {
  /*
    Possible context options:
      - init
      - next
      - init/next+check
    Assignments types:
      - eqTo: just comparison without assignment
      - assignTo: assignment with check
    Combinations:
      - check: return eq always
      - init state + init var: assign
      - next state + next var: assign
      - init state + next var: error, will be checked inside context
      - next state + init var: eq
  */
  if (ctx.isCheck()) {
    return false;
  }
  return var.getState() == ctx.getState();
}

fun(assignTo, ctx, l, r) {
  return BooleanResult::fromRaw(
      [capture(l), rval = extract(ctx, fwd(r))] lam_arg(ctx) {
        return l.assignTo(ctx, rval);
      });
}

fun(eqTo, ctx, l, r) {
  return BooleanResult{extract(ctx, fwd(l)) == extract(ctx, fwd(r))};
}

let assignOp = lam(ctx, l, r) {
  return isAssign(l, ctx) ? assignTo(ctx, fwd(l, r)) : eqTo(ctx, fwd(l, r));
};

fun(resultOf, t) {
  if_is(t, lazy) { return t(); }
  else {
    return fwd(t);
  }
}

tname(L, R) using BinaryBooleanResultType =
    std::conditional_t<is_eq<L, R>, std::decay_t<L>, BooleanResult>;

static_assert(is_eq<bool, BinaryBooleanResultType<bool, bool>>);
static_assert(is_eq<BooleanResult,
                    BinaryBooleanResultType<BooleanResult, BooleanResult>>);
static_assert(
    is_eq<LogicResult, BinaryBooleanResultType<LogicResult, LogicResult>>);
static_assert(
    is_eq<BooleanResult, BinaryBooleanResultType<bool, BooleanResult>>);
static_assert(
    is_eq<BooleanResult, BinaryBooleanResultType<BooleanResult, bool>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<LogicResult, bool>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<bool, LogicResult>>);

fun(binaryBooleanOp, impl, l, r) {
  // If any of lam_arg is bool, evaluate impl with first lam_arg as bool.
  if_eq(resultOf(fwd(l)), bool) { return impl(fwd(l, r)); }
  else if_eq(resultOf(fwd(r)), bool) {
    return impl(fwd(r, l));
  }
  else if_eq(resultOf(fwd(l)), BooleanResult) {
    // Visit l and try again.
    return std::visit(
        [&r, &impl] lam_arg(v) -> BooleanResult {
          return binaryBooleanOp(fwd(impl, v, r));
        },
        resultOf(fwd(l)));
  }
  else if_eq(resultOf(fwd(r)), BooleanResult) {
    // extract right.
    return binaryBooleanOp(fwd(impl, r, l));
  }
  else {
    return impl(fwd(r, l));
  }
}

fun(doAndSimple, l, r) {
  return AssignsResult{
      [capture(l, r)] lam_arg(ctx) { return l(ctx) && r(ctx); }};
}

fun(concatVectors, l, r) {
  using namespace std;
  if_is(l, rvalue_reference_v) {
    l.insert(l.end(), r.begin(), r.end());
    return fwd(l);
  }
  else {
    auto v = l;
    v.insert(v.end(), r.begin(), r.end());
    return v;
  }
}

fun(mulVectorsImpl, ls, rs) {
  LogicResult res;
  for (auto&& l : ls) {
    for (auto&& r : rs) {
      res.push_back(doAndSimple(fwd(l, r)));
    }
  }
  return res;
}

fun(mulVectors, ls, rs) {
  using namespace std;
  if_is(ls, rvalue_reference_v) {
    if (ls.size() == 1 && rs.size() == 1) {
      ls[0] = doAndSimple(std::move(ls[0]), fwd(rs[0]));
      return LogicResult{std::move(ls)};
    } else {
      return mulVectorsImpl(fwd(ls, rs));
    }
  }
  else {
    return mulVectorsImpl(fwd(ls, rs));
  }
}

fun(orImpl, l, r) {
  if_eq(resultOf(fwd(l)), bool) {
    return resultOf(fwd(l))
               ? BinaryBooleanResultType<decltype(resultOf(fwd(l))),
                                         decltype(resultOf(fwd(r)))>{true}
               : resultOf(fwd(r));
  }
  else {
    return concatVectors(resultOf(fwd(l)), resultOf(fwd(r)));
  }
}

fun(andImpl, l, r) {
  if_eq(resultOf(fwd(l)), bool) {
    return resultOf(fwd(l))
               ? BinaryBooleanResultType<decltype(resultOf(fwd(l))),
                                         decltype(resultOf(fwd(r)))>{resultOf(
                     fwd(r))}
               : false;
  }
  else {
    return mulVectors(resultOf(fwd(l)), resultOf(fwd(r)));
  }
}

fun(orOp, l, r) { return binaryBooleanOp(as_lam(orImpl), fwd(l, r)); }
fun(andOp, l, r) { return binaryBooleanOp(as_lam(andImpl), fwd(l, r)); }

}  // namespace detail

inline bool operator!(const LogicResult&) {
  throw ExpressionError("Negative operation cannot be applied to logic result");
}

inline bool operator!(const BooleanResult& b) {
  return std::visit(lam_in(v, !v), b);
}

fun_if(operator==, is_any_of(is_expression, T_l, T_r), l, r) {
  if_is(l, assignment) {
    // Left argument can be used in special assignment procedure.
    return evaluate_ctx(detail::assignOp, fwd(l, r));
  }
  else {
    // If assignment is not allowed, do just simple comparison.
    return evaluator(l == r, l, r);
  }
}

fun_if(operator||, is_any_of(is_expression, T_l, T_r), l, r) {
  return evaluator_lazy_fun(detail::orOp, l, r);
}

fun_if(operator&&, is_any_of(is_expression, T_l, T_r), l, r) {
  return evaluator_lazy_fun(detail::andOp, l, r);
}
