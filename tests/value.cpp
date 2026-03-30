#include "value.h"

#include <gtest/gtest.h>

namespace test {

TEST(Values, Create) {
  Value i{1, "i"};
  Value s{std::string("ab"), "s"};
}

TEST(Values, Compare) {
  Value i1{1, "i1"};
  Value i2{2, "i2"};
  Value i3{1, "i3"};

  ASSERT_EQ(i1, i3);
  ASSERT_NE(i1, i2);

  Value i4{i1};
  ASSERT_EQ(i4, i1);
  ASSERT_EQ(i4, i3);

  ASSERT_NE(i4, i2);
}

TEST(Values, Hash) {
  Value i1{1, "i1"};
  Value i2{2, "i2"};
  Value i3{1, "i3"};

  auto h1 = std::hash<Value>{}(i1);
  auto h2 = std::hash<Value>{}(i2);
  auto h3 = std::hash<Value>{}(i3);

  ASSERT_EQ(h1, h3);
  ASSERT_NE(h1, h2);
}

TEST(Values, TypesManip) {
  State types;
  types.emplace_back(1, "1");
  types.emplace_back(std::string{"a"}, "2");

  auto h1 = std::hash<State>{}(types);

  auto typesCopy = types;
  auto h2 = std::hash<State>{}(typesCopy);
  ASSERT_EQ(h1, h2);

  types.emplace_back(3, "3");
  auto h3 = std::hash<State>{}(types);
  ASSERT_NE(h1, h3);
}

}  // namespace test
