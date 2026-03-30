#pragma once

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "macro.h"
#include "tag.h"

namespace detail {

// Creates a container when v is not iterable, using the sample type.
fun(asContainer, v, sample) {
  if_is(v, iterable) { return fwd(v); }
  else {
    static_assert(is_iterable<decltype(sample)>, "Sample must be iterable");
    return std::decay_t<decltype(sample)>{fwd(v)};
  }
}

fun(wrapSetStdOp, f, ll, rr) {
  decltype(auto) l = asContainer(fwd(ll), rr);
  decltype(auto) r = asContainer(fwd(rr), ll);

  static_assert(is_eq<decltype(l), decltype(r)>,
                "Incompatible types: types must be equal in "
                "binary set operation");

  std::decay_t<decltype(l)> result;
  f(l.begin(), l.end(), r.begin(), r.end(),
    std::inserter(result, result.end()));
  return result;
}

fun(wrapBoolSetStdOp, f, ll, rr) {
  decltype(auto) l = asContainer(fwd(ll), rr);
  decltype(auto) r = asContainer(fwd(rr), ll);

  static_assert(is_eq<decltype(l), decltype(r)>,
                "Incompatible types: types must be equal in "
                "binary set operation");

  return f(l.begin(), l.end(), r.begin(), r.end());
}

}  // namespace detail

#define DEFINE_SET_WRAP(D_name, D_stdName, D_wrap) \
  fun(D_name, l, r) { return D_wrap(as_lam(D_stdName), fwd(l, r)); }

DEFINE_SET_WRAP(merge, std::set_union, detail::wrapSetStdOp)
DEFINE_SET_WRAP(difference, std::set_difference, detail::wrapSetStdOp)
DEFINE_SET_WRAP(symmetricDifference, std::set_symmetric_difference,
                detail::wrapSetStdOp)
DEFINE_SET_WRAP(intersection, std::set_intersection, detail::wrapSetStdOp)
DEFINE_SET_WRAP(includes, std::includes, detail::wrapBoolSetStdOp)

// Swap parameters
fun(inSet, l, r) { return includes(fwd(r, l)); }
