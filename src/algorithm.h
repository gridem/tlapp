#pragma once

#include <algorithm>

#include "evaluate.h"
#include "macro.h"

#define DEFINE_ALGORITHM(D_fun, D_stdfun)         \
  let D_fun = lam(... v) {                        \
    return evaluate(as_lam(D_stdfun), fwd(v)...); \
  };

DEFINE_ALGORITHM(min, std::min)
DEFINE_ALGORITHM(max, std::max)
