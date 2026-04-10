#pragma once

#include <type_traits>

#include "macro.h"

struct Ctor {};

let ctor = Ctor{};

tname(... T) struct Mix : T... {
  tname(... U) constexpr explicit Mix(Ctor, U&&... u) : T{fwd(u)}... {}
};

funs(mix, t) {
  return Mix<std::decay_t<T_t>...>{ctor, fwd(t)...};
}
