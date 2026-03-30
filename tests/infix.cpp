#include "infix.h"

#include <gtest/gtest.h>

#include <set>

#include "var.h"

namespace test {

TEST(Infix, In) {
  Var<std::set<int>> x{"x"};
  Var<std::set<int>> y{"y"};
  Var<int> z{"z"};

  auto e = x $in y;
  auto ez = z $in y;
  Context ctx;

  x.getRef(ctx) = {1};
  y.getRef(ctx) = {1, 2};

  EXPECT_EQ(true, e(ctx));

  x.getRef(ctx) = {1, 3};
  EXPECT_EQ(false, e(ctx));

  z.getRef(ctx) = 1;
  EXPECT_EQ(true, ez(ctx));

  z.getRef(ctx) = 2;
  EXPECT_EQ(true, ez(ctx));

  z.getRef(ctx) = 3;
  EXPECT_EQ(false, ez(ctx));
}

TEST(Infix, Cap) {
  Var<std::set<int>> x{"x"};
  Var<std::set<int>> y{"y"};

  auto e = x $cap y;
  Context ctx;
  x.getRef(ctx) = {1, 2};
  y.getRef(ctx) = {1, 3};
  EXPECT_EQ(std::set<int>({1}), e(ctx));
}

TEST(Infix, Cup) {
  Var<std::set<int>> x{"x"};
  Var<std::set<int>> y{"y"};

  auto e = x $cup y;
  Context ctx;
  x.getRef(ctx) = {1, 2};
  y.getRef(ctx) = {1, 3};
  EXPECT_EQ(std::set<int>({1, 2, 3}), e(ctx));
}

TEST(Infix, Diff) {
  Var<std::set<int>> x{"x"};
  Var<std::set<int>> y{"y"};

  auto e = x $diff y;
  Context ctx;
  x.getRef(ctx) = {1, 2};
  y.getRef(ctx) = {1, 3};
  EXPECT_EQ(std::set<int>({2}), e(ctx));
}

TEST(Infix, DiffSym) {
  Var<std::set<int>> x{"x"};
  Var<std::set<int>> y{"y"};

  auto e = x $sym_diff y;
  Context ctx;
  x.getRef(ctx) = {1, 2};
  y.getRef(ctx) = {1, 3};
  EXPECT_EQ(std::set<int>({2, 3}), e(ctx));
}

}  // namespace test
