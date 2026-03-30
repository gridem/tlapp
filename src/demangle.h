#pragma once

#include <string>
#include <type_traits>

#include "macro.h"

namespace detail {

std::string demangleImpl(const char* name);

}

tname(T) std::string typePrefix() {
  return std::is_const_v<std::remove_reference_t<T>> ? "const" : "mut";
}

tname(T) std::string typeSuffix() {
  return std::is_rvalue_reference_v<T>   ? "&&"
         : std::is_lvalue_reference_v<T> ? "&"
                                         : " val";
}

tname(T) auto mangle() { return typeid(T).name(); }

tname(T) auto demangle() {
  return typePrefix<T>() + " " + detail::demangleImpl(mangle<T>()) +
         typeSuffix<T>();
}

tname(T) auto demangle(T&&) { return demangle<T>(); }
