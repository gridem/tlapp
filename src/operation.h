#pragma once

#include "evaluate.h"

#define DEFINE_UNARY(D_op) \
  fun_if(operator D_op, is_expression<T_v>, v) { return evaluator(D_op v, v); }

#define DEFINE_BINARY(D_op)                                         \
  fun_if(operator D_op, is_any_of(is_expression, T_l, T_r), l, r) { \
    return evaluator(l D_op r, l, r);                               \
  }

DEFINE_UNARY(!)
DEFINE_UNARY(-)
DEFINE_UNARY(+)

DEFINE_BINARY(+)
DEFINE_BINARY(-)
DEFINE_BINARY(*)
DEFINE_BINARY(/)

DEFINE_BINARY(!=)
DEFINE_BINARY(<)
DEFINE_BINARY(<=)
DEFINE_BINARY(>)
DEFINE_BINARY(>=)
