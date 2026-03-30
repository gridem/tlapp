#pragma once

// Take 10th lam_arg.
#define macro_arg10(D_1, D_2, D_3, D_4, D_5, D_6, D_7, D_8, D_9, D_10, D, ...) D
// Choose macro based on number of params.
#define macro_choose(D_name, ...)                                    \
  macro_arg10(dummy, ##__VA_ARGS__, D_name##9, D_name##8, D_name##7, \
              D_name##6, D_name##5, D_name##4, D_name##3, D_name##2, \
              D_name##1, D_name##0)

#define macro_empty()
#define macro_identity(D) D macro_empty
#define macro_comma() ,
#define macro_or() ||
#define macro_and() &&

#define macro_iterate_0(D_for, D_delim)
#define macro_iterate_1(D_for, D_delim, D_1) D_for(D_1)
#define macro_iterate_2(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_1(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_3(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_2(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_4(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_3(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_5(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_4(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_6(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_5(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_7(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_6(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_8(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_7(D_for, D_delim, __VA_ARGS__)
#define macro_iterate_9(D_for, D_delim, D_1, ...) \
  D_for(D_1) D_delim() macro_iterate_8(D_for, D_delim, __VA_ARGS__)

#define macro_iterate1_0(D_for, D_arg1, D_delim)
#define macro_iterate1_1(D_for, D_arg1, D_delim, D_1) D_for(D_arg1, D_1)
#define macro_iterate1_2(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_1(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_3(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_2(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_4(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_3(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_5(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_4(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_6(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_5(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_7(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_6(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_8(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_7(D_for, D_arg1, D_delim, __VA_ARGS__)
#define macro_iterate1_9(D_for, D_arg1, D_delim, D_1, ...) \
  D_for(D_arg1, D_1) D_delim()                             \
      macro_iterate1_8(D_for, D_arg1, D_delim, __VA_ARGS__)

#define macro_rotate_0(_)
#define macro_rotate_1(_, D) D
#define macro_rotate_2(_, D1, D) D, D1
#define macro_rotate_3(_, D1, D2, D) D, D1, D2
#define macro_rotate_4(_, D1, D2, D3, D) D, D1, D2, D3
#define macro_rotate_5(_, D1, D2, D3, D4, D) D, D1, D2, D3, D4
#define macro_rotate_6(_, D1, D2, D3, D4, D5, D) D, D1, D2, D3, D4, D5
#define macro_rotate_7(_, D1, D2, D3, D4, D5, D6, D) D, D1, D2, D3, D4, D5, D6
#define macro_rotate_8(_, D1, D2, D3, D4, D5, D6, D7, D) \
  D, D1, D2, D3, D4, D5, D6, D7
#define macro_rotate_9(_, D1, D2, D3, D4, D5, D6, D7, D8, D) \
  D, D1, D2, D3, D4, D5, D6, D7, D8

// The last param becomes the first.
// Sample: macro_rotate(A, B, X) => X, A, B
#define macro_rotate(...) \
  macro_choose(macro_rotate_, ##__VA_ARGS__)(_, ##__VA_ARGS__)

// Iterate using loop macro and delim.
// Sample: macro_iterate_delim(F, D, a, b) => F(a) D() F(b)
#define macro_iterate_delim(D_for, D_delim, ...) \
  macro_choose(macro_iterate_, ##__VA_ARGS__)(D_for, D_delim, ##__VA_ARGS__)

// Iterate using loop macro.
// Sample: macro_iterate(F, a, b) => F(a) F(b)
#define macro_iterate(D_for, ...) \
  macro_iterate_delim(D_for, macro_empty, ##__VA_ARGS__)

// Iterate using loop macro with comma as separator.
// Sample: macro_iterate(F, a, b) => F(a), F(b)
#define macro_iterate_comma(D_for, ...) \
  macro_iterate_delim(D_for, macro_comma, ##__VA_ARGS__)

// Iterate using loop macro, delim and single lam_arg.
// Sample: macro_iterate_delim(F, A, D, a, b) => F(a, A) D() F(b, A)
#define macro_iterate_delim_arg(D_for, D_arg, D_delim, ...)           \
  macro_choose(macro_iterate1_, ##__VA_ARGS__)(D_for, D_arg, D_delim, \
                                               ##__VA_ARGS__)

// Iterate using loop macro.
// Sample: macro_iterate(F, A, a, b) => F(a, A) F(b, A)
#define macro_iterate_arg(D_for, D_arg, ...) \
  macro_iterate_delim_arg(D_for, D_arg, macro_empty, ##__VA_ARGS__)

// Iterate using loop macro with comma as separator.
// Sample: macro_iterate(F, A, a, b) => F(a, A), F(b, A)
#define macro_iterate_comma_arg(D_for, D_arg, ...) \
  macro_iterate_delim_arg(D_for, D_arg, macro_comma, ##__VA_ARGS__)

// Extracts value based on index
// Sample: macro_index_0 (A,B) => A
#define macro_index_0(D_0, ...) D_0
#define macro_index_1(D_0, D_1...) D_1
#define macro_index_2(D_0, D_1, D_2...) D_2

#define macro_concat_impl(D_1, D_2) D_1##D_2
// Concatenates macro
#define macro_concat(D_1, D_2) macro_concat_impl(D_1, D_2)
