#include "macro_iterator.h"

#include <gtest/gtest.h>

namespace test {

#define LOOP(D) D
#define LOOP1(D) D "1"
#define STR(D) #D
#define STRINGIZE(D) STR(D)

#define DELIM() "-"

#define ARG(D, A) D "p" A

TEST(MacroIterator, Simple) {
  ASSERT_EQ("", "" macro_iterate(LOOP));
  ASSERT_EQ("", "" macro_iterate_comma(LOOP1));
  ASSERT_EQ("abc", macro_iterate(LOOP, "a", "b", "c"));
  ASSERT_EQ(
      "123456789", macro_iterate(LOOP, "1", "2", "3", "4", "5", "6", "7", "8", "9"));
}

TEST(MacroIterator, Delim) {
  ASSERT_EQ("", "" macro_iterate_delim(LOOP, macro_identity(",")));
  ASSERT_EQ("1", "" macro_iterate_delim(LOOP, macro_identity(" "), "1"));
  ASSERT_EQ("1 2", "" macro_iterate_delim(LOOP, macro_identity(" "), "1", "2"));
  ASSERT_EQ("a,b,c", macro_iterate_delim(LOOP, macro_identity(","), "a", "b", "c"));
}

TEST(MacroIterator, Str) {
  ASSERT_EQ("(a , b , c)", STRINGIZE((macro_iterate_comma(LOOP, a, b, c))));
}

TEST(MacroIterator, Arg) {
  ASSERT_EQ("", "" macro_iterate_arg(ARG, "x"));
  ASSERT_EQ("xp1", macro_iterate_arg(ARG, "x", "1"));
  ASSERT_EQ("yp1", macro_iterate_arg(ARG, "y", "1"));
  ASSERT_EQ("xp1xp2", macro_iterate_arg(ARG, "x", "1", "2"));
  ASSERT_EQ("yp1-yp2-yp3", macro_iterate_delim_arg(ARG, "y", DELIM, "1", "2", "3"));
}

TEST(MacroRotate, Simple) {
  ASSERT_EQ("", "" macro_rotate());
  ASSERT_EQ("(b, a)", STRINGIZE((macro_rotate(a, b))));
  ASSERT_EQ("(ret, a, b)", STRINGIZE((macro_rotate(a, b, ret))));
}

}  // namespace test
