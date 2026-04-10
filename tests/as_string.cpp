#include "as_string.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace test {
namespace as_string {

TEST(AsString, String) {
  EXPECT_EQ("s", asString("s"));
  EXPECT_EQ("s", asString(std::string{"s"}));
}

TEST(AsString, Bool) {
  EXPECT_EQ("true", asString(true));
  EXPECT_EQ("false", asString(false));
}

TEST(AsString, Int) {
  EXPECT_EQ("1", asString(1));
  EXPECT_EQ("1", asString(1l));
  EXPECT_EQ("1", asString(1ll));
  EXPECT_EQ("1", asString(1u));
  EXPECT_EQ("1", asString(1ul));
  EXPECT_EQ("1", asString(1ull));
}

TEST(AsString, Floating) {
  EXPECT_EQ("1.000000", asString(1.f));
  EXPECT_EQ("1.000000", asString(1.));
}

TEST(AsString, Tuple) {
  EXPECT_EQ("{1, 2}", asString(std::make_tuple(1, 2)));
  EXPECT_EQ("{1, 2}", asString(std::make_tuple("1", 2)));
}

TEST(AsString, Pair) {
  EXPECT_EQ("1: 2", asString(std::make_pair(1, 2)));
}

TEST(AsString, Containers) {
  EXPECT_EQ("[1, 2, 3]", asString(std::vector<int>{1, 2, 3}));
  EXPECT_EQ("(1, 2, 3)", asString(std::set<int>{1, 2, 3}));
  EXPECT_EQ("{1: 2, 2: 3}", asString(std::map<int, int>{{1, 2}, {2, 3}}));
}

TEST(AsString, SingleContainers) {
  EXPECT_EQ("1", asString(std::optional<int>{1}));
  EXPECT_EQ("<>", asString(std::optional<int>{}));

  EXPECT_EQ("2", asString(std::make_unique<int>(2)));
  EXPECT_EQ("<>", asString(std::unique_ptr<int>{}));

  EXPECT_EQ("3", asString(std::make_shared<int>(3)));
  EXPECT_EQ("<>", asString(std::shared_ptr<int>{}));
}

TEST(AsString, Concat) {
  EXPECT_EQ("hello 1", asString("hello ", 1));
}

}  // namespace as_string
}  // namespace test
