#pragma once

#include <functional>

#include "macro.h"
#include "tag.h"

tname(T) size_t calcHash(const T& t) {
  if_is(t, hashable) { return t.toHash(); }
  else {
    return std::hash<std::decay_t<T>>{}(t);
  }
}