#pragma once

#include "macro_iterator.h"
#include "true_forward.h"

#define FOR_afun(D) auto&& D
#define FOR_fun_typename(D) typename T_##D
#define FOR_fun_typename_comma(D) typename T_##D,
#define FOR_fun_args(D) T_##D&& D
#define FOR_fun_args_comma(D) T_##D &&D,
#define FOR_fun_spec_typename(D) typename D
#define FOR_fwd(D) true_forward(D)
#define FOR_capture(D) D = FOR_fwd(D)
#define FOR_capture_move(D) D = std::move(D)

// Used for SFINAE checks.
#define use_if(D_bool) std::enable_if_t<D_bool, int> = 0

// Defines the variable, useful together with lam.
#define let inline constexpr decltype(auto)

// Anonymous function, lambda.
// Sample: lam_arg(a, b) => (auto&& a, auto&& b)
#define lam_arg(...) (macro_iterate_comma(FOR_afun, ##__VA_ARGS__))

// Global anonymous function with empty capture
// Sample: lam(a, b) => [](auto&& a, auto&& b)
#define lam(...) [] lam_arg(__VA_ARGS__)

// Declares set of templates.
// Sample: tname(T, ...V) =>
//      template<typename T, typename ...V>
#define tname(...) template <macro_iterate_comma(FOR_fun_spec_typename, ##__VA_ARGS__)>

// Declares set of templates.
// Sample: tname_if(I, T) =>
//      template<typename T, use_if(I)>
#define tname_if(D_if, ...) \
  template <macro_iterate_comma(FOR_fun_spec_typename, ##__VA_ARGS__), use_if(D_if)>

// Creates standalone template function with auto types.
// Sample: fun(X, a, b) =>
//      template<typename T_a, typename T_b>
//      decltype(auto) X(T_a&& a, T_b&& b)
#define fun(D_name, ...)                                          \
  template <macro_iterate_comma(FOR_fun_typename, ##__VA_ARGS__)> \
  decltype(auto) D_name(macro_iterate_comma(FOR_fun_args, ##__VA_ARGS__))

// Creates standalone template function with auto types.
// Sample: funs(X, a, b) =>
//      template<typename T_a, typename... T_b>
//      decltype(auto) X(T_a&& a, T_b&& b...)
#define IMPL_funs_1(D_name, D_last, ...)                                                 \
  template <macro_iterate(FOR_fun_typename_comma, ##__VA_ARGS__) typename... T_##D_last> \
  decltype(auto) D_name(                                                                 \
      macro_iterate(FOR_fun_args_comma, ##__VA_ARGS__) T_##D_last&&... D_last)
#define IMPL_funs_0(...) IMPL_funs_1(__VA_ARGS__)
#define funs(D_name, ...) IMPL_funs_0(D_name, macro_rotate(__VA_ARGS__))

// Defines the function if it satisfies the constexpr condition.
// Sample: fun_if(X, I, a, b) =>
//      template<typename T_a, typename T_b, use_if(I)>
//      decltype(auto) X(T_a&& a, T_b&& b)
#define fun_if(D_name, D_if, ...)                                               \
  template <macro_iterate_comma(FOR_fun_typename, ##__VA_ARGS__), use_if(D_if)> \
  decltype(auto) D_name(macro_iterate_comma(FOR_fun_args, ##__VA_ARGS__))

// Forwards the value based on the value itself instead of type.
// Sample: fwd(a, b) =>
//      std::forward<decltype(a)>(a), std::forward<decltype(b)>(b)
#define fwd(...) macro_iterate_comma(FOR_fwd, ##__VA_ARGS__)

// Captures the values by forwarding them to the capture context.
// Sample: capture(a, b) =>
//      a = std::forward<decltype(a)>(a), b = std::forward<decltype(b)>(b)
#define capture(...) macro_iterate_comma(FOR_capture, ##__VA_ARGS__)

// Captures the values by moving them to the capture context.
// Sample: capture_move(a, b) =>
//      a = std::move(a), b = std::move(b)
#define capture_move(...) macro_iterate_comma(FOR_capture_move, ##__VA_ARGS__)

/// Rename lam_in (lambda in-place)
// Creates a lambda with immediate return statement.
// Sample: lam_in(a, b, R) =>
//      [](auto&& a, auto&& b) { return R; }
#define IMPL_lam_in_1(D_fun, D_ret, ...) \
  D_fun(__VA_ARGS__) { return D_ret; }
#define IMPL_lam_in_0(...) IMPL_lam_in_1(__VA_ARGS__)
#define lam_in(...) IMPL_lam_in_0(lam, macro_rotate(__VA_ARGS__))

// Converts function to lambda object to be used as argument in other functions.
#define as_lam(D_name) [](auto&&... v) { return D_name(fwd(v)...); }

// Packs variadic args in lambda capture.
#define variadic_pack(D_var, D_call) D_var = std::make_tuple(D_call)
// Unpacks variadic args using specific call.
#define variadic_unpack(D_var, D_call) \
  std::apply([&] lam_arg(... D_var) { return D_call; }, std::move(D_var))

// Sample: if_as(value, is_expression) {} else {}
#define if_as(D_var, D_cond) if constexpr (D_cond<decltype(D_var)>)

// Sample: if_is(value, expression) {} else {}
#define if_is(D_var, D_cond) if_as(D_var, is_##D_cond)

// Sample: if_eq(value, bool) {} else {}
#define if_eq(D_var, D_type) if constexpr (is_eq<decltype(D_var), D_type>)
