#pragma once

#include <functional>
#include <variant>
#include <vector>

#include "context.h"
#include "tag.h"

namespace detail {

// Specify raw expression type without any tags.
tname(R) using RawX = std::function<R(Context&)>;

// Erasure Expression type
tname(T_expr) struct ErasureExpression : T_expr,
                                         expression_tag_type,
                                         expression_untyped_tag_type {
  tname_if(!is_expression_untyped<T>, T) ErasureExpression(T&& t) : T_expr{fwd(t)} {}
};

// Variant with either immediate result or expression result.
tname(R) using ErasureExpressionVariant = ErasureExpression<std::variant<R, RawX<R>>>;

// ErasureExpressionVariant lets an expression be either:
//  1) an immediate value R (constant expression), or
//  2) a deferred expression RawX<R> (callable R(Context&)).
// This allows constants to flow through the same expression pipeline
// without wrapping them in a lambda, reducing overhead and allocations.
// Typical evaluation: if holds_alternative<R>, return the value;
// otherwise invoke the RawX<R> with Context.
// NOTE: This alias is currently unused in the codebase.

}  // namespace detail

// Represents expression that returns type R.
tname(R) using Expression = detail::ErasureExpression<detail::RawX<R>>;

// Operation is either Expression or Boolean.
// Returns operation type extracted from lambda(Context&) call.
tname(T) using ExpressionType = std::decay_t<std::invoke_result_t<T, Context&>>;

namespace detail {

// Operand type helper overloads (declaration-only).
tname_if(is_immediate<T>, T) auto operandType(T&&) -> std::decay_t<T>;
tname_if(!is_immediate<T>, T) auto operandType(T&&) -> ExpressionType<T>;

}  // namespace detail

// Extracts the type from expression result.
#define type_result(D_expr) std::decay_t<decltype(D_expr)>
// Extracts the type from function. T type should be used as input
#define type_fun(D_fun, D_T) type_result(D_fun(std::declval<D_T>()))

// Extracts the operand type where operand can be expression or immediate (e.g.
// constant).
tname(T) using OperandType = type_fun(detail::operandType, T);

static_assert(is_eq<int, OperandType<int>>);
