#include "true_forward.h"

#include <gtest/gtest.h>

namespace test {

#define fwd true_forward

int deduce(int&) { return 1; }
int deduce(int&&) { return 2; }
int deduce(const int&) { return 3; }
int deduce(const int&&) { return 4; }
int deduce(...) { return 5; }

template <typename T>
int funDeduce(T&& t) {
  return deduce(fwd(t));
}

template <typename T>
int funDeduceStd(T&& t) {
  return deduce(std::forward<T>(t));
}

TEST(Fwd, Const) {
  EXPECT_EQ(2, deduce(3));
  EXPECT_EQ(2, funDeduce(3));
  EXPECT_EQ(2, funDeduceStd(3));
}

TEST(Fwd, Var) {
  int x = 0;

  EXPECT_EQ(1, deduce(x));
  EXPECT_EQ(1, deduce(fwd(x)));
  EXPECT_EQ(1, deduce(fwd((x))));

  EXPECT_EQ(2, deduce(std::move(x)));
  EXPECT_EQ(2, deduce(std::move(fwd(x))));
  EXPECT_EQ(2, deduce(std::move(fwd((x)))));
  EXPECT_EQ(2, deduce(fwd(std::move(x))));
  EXPECT_EQ(2, deduce(fwd((std::move(x)))));
  EXPECT_EQ(2, deduce(fwd(std::move(fwd(x)))));
}

TEST(Fwd, Fun) {
  int x = 0;

  EXPECT_EQ(1, funDeduce(x));
  EXPECT_EQ(1, funDeduce(fwd(x)));
  EXPECT_EQ(1, funDeduce(fwd((x))));

  EXPECT_EQ(2, funDeduce(std::move(x)));
  EXPECT_EQ(2, funDeduce(std::move(fwd(x))));
  EXPECT_EQ(2, funDeduce(std::move(fwd((x)))));
  EXPECT_EQ(2, funDeduce(fwd(std::move(x))));
  EXPECT_EQ(2, funDeduce(fwd((std::move(x)))));
  EXPECT_EQ(2, funDeduce(fwd(std::move(fwd(x)))));
}

TEST(Fwd, Tmp) {
  int x = 0;
  int y = 0;

  EXPECT_EQ(2, deduce(x + y));
  EXPECT_EQ(2, deduce(fwd(x + y)));
  EXPECT_EQ(2, deduce(fwd((x + y))));

  EXPECT_EQ(2, funDeduce(x + y));
  EXPECT_EQ(2, funDeduce(fwd(x + y)));
  EXPECT_EQ(2, funDeduce(fwd((x + y))));
}

}  // namespace test
