#include "operation.h"

#include <gtest/gtest.h>

#include "var.h"

namespace test {

TEST(Operation, UnMinus) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;

  auto e = -i;

  EXPECT_EQ(-2, e(ctx));
}

TEST(Operation, BinPlus) {
  Var<int> i{"i"};
  Var<int> j{"j"};

  Context ctx;
  i.getRef(ctx) = 2;
  j.getRef(ctx) = 3;

  auto s = i + j;

  EXPECT_EQ(5, s(ctx));
}

TEST(Operation, Minus) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;

  auto s = 5 - i;

  EXPECT_EQ(3, s(ctx));
}

TEST(Operation, ComparisonConst) {
  Var<int> i{"i"};

  Context ctx;
  i.getRef(ctx) = 2;

  EXPECT_EQ((i > 2)(ctx), false);
  EXPECT_EQ((i >= 2)(ctx), true);
}

TEST(Operation, ComparisonVar) {
  Var<int> i{"i"};
  Var<int> j{"j"};

  Context ctx;
  i.getRef(ctx) = 2;
  j.getRef(ctx) = 3;

  EXPECT_EQ((i > j)(ctx), false);
  EXPECT_EQ((i >= j)(ctx), false);
  EXPECT_EQ((i < j)(ctx), true);
  EXPECT_EQ((i <= j)(ctx), true);
  EXPECT_EQ((i != j)(ctx), true);
}

}  // namespace test
