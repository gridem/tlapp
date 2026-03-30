#include "functional.h"

#include <gtest/gtest.h>

#include "operation.h"
#include "var.h"

namespace test {

TEST(Functional, Unchanged) {
  Var<int> x{"x"};
  Context ctx;
  x.getRef(ctx) = 1;
  ctx.setState(LogicState::Next);

  auto e = unchanged(x);
  ASSERT_FALSE(x++.getRef(ctx));

  ASSERT_TRUE(e(ctx).evaluate(ctx));
  ASSERT_TRUE(x++.getRef(ctx));
  EXPECT_EQ(1, *x++.getRef(ctx));
}

TEST(Functional, UnchangedMany) {
  Var<int> x{"x"};
  Var<int> y{"y"};
  Var<int> z{"z"};
  Context ctx;
  x.getRef(ctx) = 1;
  y.getRef(ctx) = 2;
  z.getRef(ctx) = 3;

  ctx.setState(LogicState::Next);

  auto e = unchanged(x, y, z);
  ASSERT_FALSE(x++.getRef(ctx));
  ASSERT_FALSE(y++.getRef(ctx));
  ASSERT_FALSE(z++.getRef(ctx));

  ASSERT_TRUE(e(ctx).evaluate(ctx));
  ASSERT_TRUE(x++.getRef(ctx));
  EXPECT_EQ(1, *x++.getRef(ctx));
  ASSERT_TRUE(y++.getRef(ctx));
  EXPECT_EQ(2, *y++.getRef(ctx));
  ASSERT_TRUE(z++.getRef(ctx));
  EXPECT_EQ(3, *z++.getRef(ctx));
}

TEST(Functional, FilterContainer) {
  std::vector<int> v{1, 2, 3};
  auto r1 = detail::filterContainerOp(v, [](int x) { return x >= 2; });
  auto r2 = detail::filterContainerOp(v, [](int x) { return x == 1; });

  EXPECT_EQ((std::vector<int>{2, 3}), r1);
  EXPECT_EQ((std::vector<int>{1}), r2);
}

TEST(Functional, FilterSetContainer) {
  std::set<int> v{1, 2, 3};
  auto r1 = detail::filterContainerOp(v, [](int x) { return x >= 2; });
  auto r2 = detail::filterContainerOp(v, [](int x) { return x == 1; });

  EXPECT_EQ((std::set<int>{2, 3}), r1);
  EXPECT_EQ((std::set<int>{1}), r2);
}

TEST(Functional, Filter) {
  Var<std::vector<int>> v("v");
  auto e1 = filter(v, [](auto&& x) { return x >= 2; });
  auto e2 = filter(v, [](auto&& x) { return x == 1; });

  Context ctx;
  v.getRef(ctx) = std::vector<int>{1, 2, 3};

  EXPECT_EQ((std::vector<int>{2, 3}), e1(ctx));
  EXPECT_EQ((std::vector<int>{1}), e2(ctx));
}

TEST(Functional, FilterSet) {
  Var<std::set<int>> v("v");
  auto e1 = filter(v, [](auto&& x) { return x >= 2; });
  auto e2 = filter(v, [](auto&& x) { return x == 1; });

  Context ctx;
  v.getRef(ctx) = std::set<int>{1, 2, 3};

  EXPECT_EQ((std::set<int>{2, 3}), e1(ctx));
  EXPECT_EQ((std::set<int>{1}), e2(ctx));
}

TEST(Functional, FilterIf) {
  Var<std::set<int>> v("v");
  auto e1 = $if(x, v) { return x >= 2; };

  Context ctx;
  v.getRef(ctx) = std::set<int>{1, 2, 3};

  EXPECT_EQ((std::set<int>{2, 3}), e1(ctx));
}

}  // namespace test
