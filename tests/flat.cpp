#include "flat.h"

#include <gtest/gtest.h>

#include <string>

namespace test {

TEST(FlatSet, SortsAndDeduplicatesInsertedValues) {
  FlatSet<int> values{3, 1, 2, 3, 2, 1};

  ASSERT_EQ(values.size(), 3);
  EXPECT_EQ(values.at(0), 1);
  EXPECT_EQ(values.at(1), 2);
  EXPECT_EQ(values.at(2), 3);

  auto [it, inserted] = values.insert(2);
  EXPECT_FALSE(inserted);
  EXPECT_EQ(*it, 2);
}

TEST(FlatSet, CopyMoveAndEraseWorkForLargerSets) {
  FlatSet<int> values;
  for (int i = 11; i >= 0; --i) {
    values.insert(i);
  }

  FlatSet<int> copied{values};
  FlatSet<int> moved{std::move(copied)};
  EXPECT_EQ(values, moved);

  EXPECT_EQ(moved.erase(7), 1);
  EXPECT_EQ(moved.erase(7), 0);
  EXPECT_FALSE(moved.contains(7));
  EXPECT_TRUE(moved.contains(8));
}

TEST(FlatMap, StoresKeysInOrderAndRejectsDuplicates) {
  FlatMap<int, std::string> values;

  EXPECT_TRUE(values.insert({3, "c"}).second);
  EXPECT_TRUE(values.insert({1, "a"}).second);
  values[2] = "b";

  auto [it, inserted] = values.insert({2, "override"});
  EXPECT_FALSE(inserted);
  EXPECT_EQ(it->second, "b");

  ASSERT_EQ(values.size(), 3);
  EXPECT_EQ(values.begin()->first, 1);
  EXPECT_EQ((values.begin() + 1)->first, 2);
  EXPECT_EQ((values.begin() + 2)->first, 3);
  EXPECT_EQ(values.at(2), "b");
}

TEST(FlatMap, CopyMoveEraseAndAtWorkForLargerMaps) {
  FlatMap<int, std::string> values;
  for (int i = 0; i < 12; ++i) {
    values.insert({i, std::to_string(i)});
  }

  FlatMap<int, std::string> copied{values};
  FlatMap<int, std::string> moved{std::move(copied)};
  EXPECT_EQ(values, moved);

  EXPECT_EQ(moved.erase(4), 1);
  EXPECT_EQ(moved.erase(4), 0);
  EXPECT_THROW(moved.at(4), std::out_of_range);
  EXPECT_EQ(moved.at(5), "5");
}

}  // namespace test
