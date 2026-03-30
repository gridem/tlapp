#include "set.h"

#include <gtest/gtest.h>

#include <set>

#include "as_string.h"
#include "operation.h"

namespace test {

using SetCRef = const std::set<int>&;

#define DEFINE_TEST_OP(D_name, D_expectedType)                              \
  let test_typed_##D_name = lam(s1, s2, expected) {                         \
    EXPECT_EQ(expected, D_name(s1, s2))                                     \
        << "s1: " << asString(s1) << ", s2: " << asString(s2);              \
  };                                                                        \
                                                                            \
  let test_##D_name = [](SetCRef s1, SetCRef s2, D_expectedType expected) { \
    test_typed_##D_name(s1, s2, expected);                                  \
  };

DEFINE_TEST_OP(merge, SetCRef)
DEFINE_TEST_OP(difference, SetCRef)
DEFINE_TEST_OP(symmetricDifference, SetCRef)
DEFINE_TEST_OP(intersection, SetCRef)
DEFINE_TEST_OP(inSet, bool)

TEST(Operation, Merge) {
  test_merge({}, {}, {});
  test_merge({}, {3}, {3});
  test_merge({1, 2}, {3}, {1, 2, 3});
  test_merge({1, 2}, {1, 3}, {1, 2, 3});
  test_merge({1, 2, 3}, {1, 2, 3}, {1, 2, 3});
}

TEST(Operation, Difference) {
  test_difference({}, {}, {});
  test_difference({}, {3}, {});
  test_difference({3}, {}, {3});
  test_difference({1, 2}, {3}, {1, 2});
  test_difference({1, 2}, {1, 3}, {2});
  test_difference({1, 2, 3}, {1, 2, 3}, {});
}

TEST(Operation, SymmetricDifference) {
  test_symmetricDifference({}, {}, {});
  test_symmetricDifference({}, {3}, {3});
  test_symmetricDifference({3}, {}, {3});
  test_symmetricDifference({1, 2}, {3}, {1, 2, 3});
  test_symmetricDifference({1, 2}, {1, 3}, {2, 3});
  test_symmetricDifference({1, 2, 3}, {1, 2, 3}, {});
}

TEST(Operation, Intersection) {
  test_intersection({}, {}, {});
  test_intersection({}, {3}, {});
  test_intersection({1, 2}, {3}, {});
  test_intersection({1, 2}, {1, 3}, {1});
  test_intersection({1, 2, 3}, {1, 2, 3}, {1, 2, 3});
}

TEST(Operation, InSet) {
  test_inSet({}, {}, true);
  test_inSet({}, {3}, true);
  test_inSet({3}, {}, false);
  test_inSet({3}, {1, 2}, false);
  test_inSet({1, 3}, {1, 2}, false);
  test_inSet({1}, {1, 2}, true);
  test_inSet({1, 2, 3}, {1, 2, 3}, true);
}

TEST(Operation, Conversion) {
  test_typed_merge(std::set<int>{}, 3, std::set<int>{3});
  test_typed_inSet(1, std::set<int>{1, 2}, true);
}

}  // namespace test
