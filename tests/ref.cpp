#include <gtest/gtest.h>

#include <functional>
#include <unordered_map>

#include "macro.h"
#include "std.h"

namespace test {

TEST(Ref, Simple) {
  int i = 2;
  std::unordered_map<std::reference_wrapper<int>, int> m;

  m[std::ref(i)] = 3;

  int i2 = 2;

  EXPECT_EQ(3, m[std::ref(i2)]);

  i2 = 1;
  EXPECT_EQ(0, m[std::ref(i2)]);
}

}  // namespace test
