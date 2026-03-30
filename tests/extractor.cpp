#include "extractor.h"

#include <gtest/gtest.h>

#include "operation.h"

namespace test {

TEST(Extractor, GetMember) {
  Var<std::vector<int>> x{"x"};
  auto e = get_mem(x, size());

  Context ctx;
  x.getRef(ctx) = {{1, 2}};
  EXPECT_EQ(2, e(ctx));

  x.getRef(ctx) = {{}};
  EXPECT_EQ(0, e(ctx));
}

TEST(Extractor, Index) {
  Var<std::vector<int>> x{"x"};
  auto e0 = at(x, 0);
  auto e1 = at(x, 1);

  Context ctx;
  x.getRef(ctx) = {1, 2};
  EXPECT_EQ(1, e0(ctx));
  EXPECT_EQ(2, e1(ctx));

  x.getRef(ctx) = {3, -3};
  EXPECT_EQ(0, (e0 + e1)(ctx));
}

TEST(Extractor, Mutate) {
  Var<int> x{"x"};
  Context ctx;
  x.getRef(ctx) = 1;
  ctx.setState(LogicState::Next);

  auto e = mut(x, lam_in(x, x + 1));
  // Next x is not set.
  EXPECT_FALSE(x++.getRef(ctx));

  EXPECT_TRUE(e(ctx).evaluate(ctx));
  ASSERT_TRUE(x++.getRef(ctx));
  EXPECT_EQ(2, *x++.getRef(ctx));
}

TEST(Extractor, MapAt) {
  Var<std::map<int, int>> m{"m"};
  Var<int> v{"v"};

  auto e = at(m, v, 2);

  Context ctx;
  m.getRef(ctx) = {{{1, 1}}};
  v.getRef(ctx) = 1;

  EXPECT_EQ(1, e(ctx).size());
  EXPECT_EQ(2, e(ctx).at(1));

  v.getRef(ctx) = 2;
  EXPECT_EQ(2, e(ctx).size());
  EXPECT_EQ(2, e(ctx).at(2));
}

TEST(Extractor, MutAt) {
  Var<std::map<int, int>> m{"m"};

  auto e1 = mutAt(m, 1, 2);
  auto e2 = mutAt(m, 2, 3);

  Context ctx;
  m.getRef(ctx) = {{{1, 1}}};
  ctx.setState(LogicState::Next);

  EXPECT_EQ(true, e1(ctx).evaluate(ctx));
  EXPECT_EQ((std::map<int, int>{{1, 2}}), m++.getRef(ctx));

  ctx.nexts().clearValues();
  EXPECT_EQ(true, e2(ctx).evaluate(ctx));
  EXPECT_EQ((std::map<int, int>{{1, 1}, {2, 3}}), m++.getRef(ctx));
}

}  // namespace test
