#pragma once

#include "evaluate.h"
#include "macro.h"
#include "mix.h"
#include "set.h"
#include "tag.h"

#define DEFINE_INFIX(D_infix, D_fun)                        \
  DEFINE_TAG(D_infix##_infix)                               \
                                                            \
  tname(T) let operator%(T&& t, D_infix##_infix_tag_type) { \
    return detail::prepare(fwd(t)) & D_infix##_infix_tag;   \
  }                                                         \
                                                            \
  fun_if(operator%, is_##D_infix##_infix<T_l>, l, r) {      \
    return evaluator_fun(D_fun, l, r);                      \
  }

DEFINE_INFIX(in, inSet)
DEFINE_INFIX(cup, merge)
DEFINE_INFIX(cap, intersection)
DEFINE_INFIX(diff, difference)
DEFINE_INFIX(sym_diff, symmetricDifference)

#define $in % in_infix_tag %
#define $cup % cup_infix_tag %
#define $cap % cap_infix_tag %
#define $diff % diff_infix_tag %
#define $sym_diff % sym_diff_infix_tag %
