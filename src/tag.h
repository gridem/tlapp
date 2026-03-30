#pragma once

#include "macro.h"
#include "traits.h"

#define DEFINE_TAG(D_tag)                                 \
  struct D_tag##_tag_type {};                             \
                                                          \
  let D_tag##_tag = D_tag##_tag_type{};                   \
                                                          \
  tname(T) let is_##D_tag = has_tag<T, D_tag##_tag_type>; \
                                                          \
  tname(T) let operator&(T&& t, D_tag##_tag_type) {       \
    return mix(fwd(t), D_tag##_tag);                      \
  }

tname(T, T_tag) let has_tag = std::is_convertible_v<std::decay_t<T>, T_tag>;

// Can be used in expression context
DEFINE_TAG(expression)
// Represents untyped (type erasure) expression.
DEFINE_TAG(expression_untyped)
// Represents boolean expression
DEFINE_TAG(boolean)
// Is variable
DEFINE_TAG(var)
// Can be used in left-handed assignment expression
DEFINE_TAG(assignment)
// Defines if type is hashable
DEFINE_TAG(hashable)
// For lazy evaluation functions
DEFINE_TAG(lazy)

#define FOR_of(D_of, D) D_of<D>
#define is_any_of(D_of, ...) \
  macro_iterate_delim_arg(FOR_of, D_of, macro_or, ##__VA_ARGS__)
#define is_all_of(D_of, ...) \
  macro_iterate_delim_arg(FOR_of, D_of, macro_and, ##__VA_ARGS__)

// Logic: if Expression then Immediate expression. Consts don't have any tags.
tname(T) let is_immediate = !is_expression<T>;
