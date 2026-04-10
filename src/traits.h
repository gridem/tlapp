#pragma once

#include <type_traits>

#include "macro.h"
#include "mix.h"

tname(T, V) let is_eq = std::is_same_v<std::decay_t<T>, std::decay_t<V>>;

tname(T) let is_bool = std::is_same_v<T, bool>;

tname(L, R) let is_compatible = std::is_same_v<L, R> || std::is_convertible_v<L, R>;

namespace detail {

tname(...) using to_void = void;
tname(T, = void) struct is_iterable_impl : std::false_type {};
tname(T) struct is_iterable_impl<T,
    to_void<decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end()),
        typename T::value_type>> : std::true_type {};

}  // namespace detail

tname(T) let is_iterable = detail::is_iterable_impl<std::decay_t<T>>::value;
