#include "tag.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace test {

TEST(Tag, iterable) {
  static_assert(is_iterable<std::vector<int>>);
  static_assert(is_iterable<std::vector<int>&&>);
  static_assert(is_iterable<std::set<int>>);
  static_assert(!is_iterable<int>);
  static_assert(!is_iterable<int&>);
}

}  // namespace test
