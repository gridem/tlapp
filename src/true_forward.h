#pragma once

#include <type_traits>

// The purpose of this file is introducing the true perfect forwarding
// mechanism. I was really surprised that std::forward under some context cannot
// satisfy perfect forwarding guarantee.
//
// See historical reasons below.

/*

In generic lambda context we usually use the following:

    auto lam = [](auto&& v) {
        // Forwarding for fun.
        fun(std::forward<???>(v));
    };

Since we don't know the type due to `auto` we should use:

    fun(std::forward<decltype(v)>(v));

It seems evident that std::forward<decltype(v)>(v) should be replaced with
shorter form:

    #define fwd(v) std::forward<decltype(v)>(v)

    fun(fwd(v));

So far so good. Now we are going to use this always in different contexts. Let's
assume that we have a variable and we want to forward it:

    int v;
    lam(fwd(v));

Now the question here: what is the type? Surprisingly, the type is int&& instead
of expected int& because forward in this context doesn't expect to accept the
type int: std::forward<decltype(v)>(v) -> std::forward<int>(v) -> int&&.

So forward is limited in usage only with auto&& or T&& t to forward from input
args, not to forward arbitrary expression/variables. The function trueForward
further generalize forwarding taking into account normal variables.
*/

template <typename T_decltype, typename T>
constexpr decltype(auto) trueForward(T&& t) noexcept {
  if constexpr (std::is_reference_v<T_decltype>) {
    // Fallback to std definition.
    return std::forward<T_decltype>(t);
  } else {
    // It's lvalue, must preserve it.
    return std::forward<T>(t);
  }
}

#define true_forward(v) trueForward<decltype(v)>(v)
