#include "value.h"

#include <array>

#include <gtest/gtest.h>

namespace test {

struct BigValue {
  std::array<int, 64> items{};

  bool operator==(const BigValue& other) const = default;

  std::string toString() const {
    return "BigValue";
  }
};

TEST(Values, Create) {
  Value i{1, "i"};
  Value s{std::string("ab"), "s"};
}

TEST(Values, Compare) {
  Value i1{1, "i1"};
  Value i2{2, "i2"};
  Value i3{1, "i3"};

  ASSERT_EQ(i1, i3);
  ASSERT_NE(i1, i2);

  Value i4{i1};
  ASSERT_EQ(i4, i1);
  ASSERT_EQ(i4, i3);

  ASSERT_NE(i4, i2);
}

TEST(Values, Hash) {
  Value i1{1, "i1"};
  Value i2{2, "i2"};
  Value i3{1, "i3"};

  auto h1 = std::hash<Value>{}(i1);
  auto h2 = std::hash<Value>{}(i2);
  auto h3 = std::hash<Value>{}(i3);

  ASSERT_EQ(h1, h3);
  ASSERT_NE(h1, h2);
}

TEST(Values, TypesManip) {
  State types;
  types.emplace_back(1, "1");
  types.emplace_back(std::string{"a"}, "2");

  auto h1 = std::hash<State>{}(types);

  auto typesCopy = types;
  auto h2 = std::hash<State>{}(typesCopy);
  ASSERT_EQ(h1, h2);

  types.emplace_back(3, "3");
  auto h3 = std::hash<State>{}(types);
  ASSERT_NE(h1, h3);
}

TEST(Values, InlineStorageCopyMove) {
  Value left{42, "left"};
  Value copied{left};
  Value moved{std::move(copied)};

  ASSERT_EQ(left, moved);

  Value assigned{0, "assigned"};
  assigned = left;
  ASSERT_EQ(assigned, left);
}

TEST(Values, HeapFallbackCopyMove) {
  Value left{BigValue{}, "big"};
  Value copied{left};
  Value moved{std::move(copied)};

  ASSERT_EQ(left, moved);

  Value assigned{BigValue{}, "assigned"};
  assigned = left;
  ASSERT_EQ(assigned, left);
}

}  // namespace test

namespace std {

template <>
struct hash<test::BigValue> {
  size_t operator()(const test::BigValue& value) const noexcept {
    size_t h = 0x9badbed;
    for (auto&& item : value.items) {
      h <<= 1;
      h ^= calcHash(item);
    }
    return h;
  }
};

}  // namespace std
