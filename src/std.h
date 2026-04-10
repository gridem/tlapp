#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "hash.h"
#include "macro.h"

namespace std {

tname(T) struct hash<unique_ptr<T>> {
  size_t operator()(const unique_ptr<T>& t) const noexcept {
    return calcHash(*t);
  }
};

tname(T) struct equal_to<unique_ptr<T>> {
  constexpr bool operator()(const unique_ptr<T>& l, const unique_ptr<T>& r) const {
    return *l == *r;
  }
};

tname(T, ... U) struct hash<vector<T, U...>> {
  size_t operator()(const vector<T, U...>& ts) const noexcept {
    size_t h = 0xbadbed;
    for (auto&& t : ts) {
      h <<= 1;
      h ^= calcHash(t);
    }
    return h;
  }
};

tname(T, ... U) struct hash<set<T, U...>> {
  size_t operator()(const set<T, U...>& ts) const noexcept {
    size_t h = 0x2badbed;
    for (auto&& t : ts) {
      h <<= 1;
      h ^= calcHash(t);
    }
    return h;
  }
};

tname(F, S) struct hash<pair<F, S>> {
  size_t operator()(const pair<F, S>& p) const noexcept {
    size_t h = 0x7badbed;
    h ^= calcHash(p.first);
    h <<= 1;
    h ^= calcHash(p.second);
    return h;
  }
};

tname(K, V, ... U) struct hash<map<K, V, U...>> {
  size_t operator()(const map<K, V, U...>& ts) const noexcept {
    size_t h = 0x6badbed;
    for (auto&& t : ts) {
      h <<= 1;
      h ^= calcHash(t);
    }
    return h;
  }
};

tname(... T) struct hash<tuple<T...>> {
  using Tuple = tuple<T...>;

  size_t operator()(const Tuple& t) const noexcept {
    return apply<0, tuple_size<Tuple>::value>(0x3badbed, t);
  }

 private:
  template <size_t I, size_t N>
  static size_t apply(size_t seed, const Tuple& t) noexcept {
    if constexpr (I == N) {
      return seed;
    } else {
      return apply<I + 1, N>((seed << 1) ^ get<I>(t), t);
    }
  }
};

tname(T) struct hash<reference_wrapper<T>> {
  size_t operator()(const reference_wrapper<T>& t) const noexcept {
    return std::hash<T>{}(t.get());
  }
};

tname(T) struct equal_to<reference_wrapper<T>> {
  constexpr bool operator()(const reference_wrapper<T>& l,
      const reference_wrapper<T>& r) const {
    return l.get() == r.get();
  }
};

}  // namespace std
