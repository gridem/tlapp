#include "inplace_vector.h"

#include <gtest/gtest.h>

namespace test {

TEST(InplaceVector, InsertErasePreservesOrder) {
  InplaceVector<int> values{1, 3, 5};

  values.insert(values.begin() + 1, 2);
  values.insert(values.begin() + 3, 4);
  values.erase(values.begin() + 2);

  ASSERT_EQ(values.size(), 4);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 4);
  EXPECT_EQ(values[3], 5);
}

TEST(InplaceVector, CopyAndMoveWorkAcrossInlineAndHeapStorage) {
  InplaceVector<int> inlineValues;
  for (int i = 0; i < 6; ++i) {
    inlineValues.push_back(i);
  }

  InplaceVector<int> inlineCopy{inlineValues};
  InplaceVector<int> inlineMoved{std::move(inlineCopy)};
  EXPECT_EQ(inlineValues, inlineMoved);

  InplaceVector<int> heapValues;
  for (int i = 0; i < 12; ++i) {
    heapValues.push_back(i);
  }

  InplaceVector<int> heapCopy{heapValues};
  InplaceVector<int> heapMoved{std::move(heapCopy)};
  EXPECT_EQ(heapValues, heapMoved);
}

TEST(InplaceVector, AtChecksBounds) {
  InplaceVector<int> values{1, 2, 3};

  EXPECT_EQ(values.at(2), 3);
  EXPECT_THROW(values.at(3), std::out_of_range);
}

}  // namespace test
