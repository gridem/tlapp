#include "macro.h"

#include <gtest/gtest.h>

namespace test {

#define STR(D) #D
#define STRINGIZE(D) STR(D)

std::string normalizeStr(const std::string& s) {
  auto res = s;
  std::string::size_type n = 0;
  while ((n = res.find(" ", n)) != std::string::npos) {
    res.erase(n, 1);
  }
  return res;
}

void TestStr(const std::string& s1, const std::string& s2) {
  EXPECT_EQ(normalizeStr(s1), normalizeStr(s2));
}

TEST(Str, Normalize) {
  EXPECT_EQ("hh", normalizeStr("h h "));
  EXPECT_EQ("template<typenameT>", normalizeStr("template <typename T>"));

  TestStr("a b", "ab ");
}

TEST(Macro, Fun) {
  TestStr("(template <typename T_a> decltype(auto) f(T_a&& a))", STRINGIZE((fun(f, a))));
  TestStr(
      "(template <typename T_a, typename T_b> decltype(auto) u(T_a&& a, T_b&& "
      "b))",
      STRINGIZE((fun(u, a, b))));
}

TEST(Macro, Funs) {
  TestStr("(template <typename... T_v> decltype(auto) f( T_v&& ...v))",
      STRINGIZE((funs(f, v))));
  TestStr(
      "(template <typename T_a, typename... T_v> decltype(auto) f(T_a&& a, "
      "T_v&& ...v))",
      STRINGIZE((funs(f, a, v))));
}

#define MY_TEST_MACRO(X) macro_index_0 X macro_concat(macro_index_1 X, _);

TEST(Macro, ArgIndex) { TestStr("(int a_;)", STRINGIZE((MY_TEST_MACRO((int, a))))); }

}  // namespace test
