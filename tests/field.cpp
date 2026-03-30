#include "field.h"

#include <gtest/gtest.h>

#include "value.h"

namespace test {

struct Sab : hashable_tag_type {
  Sab(int a_ = 0, int b_ = 0) : a{a_}, b{b_} {}

  int a;
  int b;

  fields(a, b)
};

struct Svec : Sab {
  std::vector<int> vec;
  Sab ab;

  fields(a, b, vec)
};

TEST(Field, asString) {
  Sab ab;
  ab.a = 1;
  ab.b = 2;
  ASSERT_EQ("{a: 1, b: 2}", asString(ab));
}

TEST(Field, asStringVec) {
  Svec s;
  ASSERT_EQ("{a: 0, b: 0, vec: []}", asString(s));
}

TEST(Field, AsHash) {
  Sab ab;
  ab.a = 2;
  ab.b = 3;
  auto h23 = ab.toHash();
  ab.a = 0;
  auto h03 = ab.toHash();
  ASSERT_NE(h23, h03);
  auto std03 = calcHash(ab);
  ASSERT_EQ(h03, std03);
}

TEST(Field, Set) {
  Sab s1{1, 2};
  Sab s2{1, 2};
  Sab s3{1, 0};
  Sab s4{1, 3};
  Sab s5{0, 1};
  Sab s6{2, 2};

  ASSERT_EQ(true, s1 == s2);
  ASSERT_EQ(false, s1 == s3);
  ASSERT_EQ(false, s1 == s4);
  ASSERT_EQ(false, s1 == s5);

  ASSERT_EQ(false, s1 < s2);
  ASSERT_EQ(false, s2 < s1);
  ASSERT_EQ(true, s3 < s1);
  ASSERT_EQ(false, s1 < s3);
  ASSERT_EQ(true, s1 < s4);
  ASSERT_EQ(false, s1 < s5);
  ASSERT_EQ(true, s1 < s6);
}

}  // namespace test
