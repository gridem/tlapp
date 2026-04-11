#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "flat.h"
#include "inplace_vector.h"
#include "macro.h"

tname(T) std::string asString(const T& t) {
  return t.toString();
}

// String/char functions.
std::string asString(char c);
std::string asString(const char* c);

const std::string& asString(bool w);

std::string asString(const std::string& t);

#define AS_STR(D_type)                    \
  inline std::string asString(D_type v) { \
    return std::to_string(v);             \
  }

#define INT_AS_STR(D_bits) \
  AS_STR(int##D_bits##_t)  \
  AS_STR(uint##D_bits##_t)

INT_AS_STR(8)
INT_AS_STR(16)
INT_AS_STR(32)
INT_AS_STR(64)
AS_STR(double)
AS_STR(float)
AS_STR(size_t)
AS_STR(ssize_t)

namespace detail {

template <std::size_t I, std::size_t N, typename T>
void tupleAsString(std::string& s, const T& t) {
  if constexpr (I < N) {
    if constexpr (I > 0) {
      s += ", ";
    }
    s += asString(std::get<I>(t));
    tupleAsString<I + 1, N>(s, t);
  }
}

}  // namespace detail

tname(... T) std::string asString(const std::tuple<T...>& t) {
  std::string result = "{";
  detail::tupleAsString<0, std::tuple_size_v<std::tuple<T...>>>(result, t);
  result += "}";
  return result;
}

tname(K, V) std::string asString(const std::pair<K, V>& p) {
  return asString(p.first) + ": " + asString(p.second);
}

tname(T) std::string containerAsString(const T& container,
    const std::string& left,
    const std::string& right) {
  std::string result;
  for (auto&& t : container) {
    if (!result.empty()) {
      result += ", ";
    }
    result += asString(t);
  }
  return left + result + right;
}

#define CONTAINER_AS_STRING(D_type, D_left, D_right)                 \
  tname(... T) std::string asString(const D_type<T...>& container) { \
    return containerAsString(container, D_left, D_right);            \
  }

CONTAINER_AS_STRING(std::vector, "[", "]")
CONTAINER_AS_STRING(std::set, "(", ")")
CONTAINER_AS_STRING(std::map, "{", "}")
CONTAINER_AS_STRING(FlatSet, "(", ")")
CONTAINER_AS_STRING(FlatMap, "{", "}")

template <typename T, size_t N>
std::string asString(const InplaceVector<T, N>& container) {
  return containerAsString(container, "[", "]");
}

#define SINGLE_AS_STRING(D_type)                             \
  tname(... T) std::string asString(const D_type<T...>& t) { \
    return t ? asString(*t) : "<>";                          \
  }

SINGLE_AS_STRING(std::unique_ptr)
SINGLE_AS_STRING(std::shared_ptr)
SINGLE_AS_STRING(std::optional)

tname(T) std::string asStringQuote(const T& t) {
  return '"' + asString(t) + '"';
}

tname(T, ... V) std::string asString(const T& t, V&&... v) {
  return asString(t) + asString(fwd(v)...);
}
