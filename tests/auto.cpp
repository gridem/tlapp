#include <glog/logging.h>
#include <gtest/gtest.h>

namespace test {
namespace deduction {

struct A {
  int x;
  int y;
};

inline constexpr auto make1 = []() { return A{1, 2}; };
inline constexpr decltype(auto) make2 = []() { return A{1, 2}; };
inline constexpr auto make3 = []() -> decltype(auto) { return A{1, 2}; };

template <typename T>
T&& forward1(T&& t) {
  return std::forward<T>(t);
}
template <typename T>
decltype(auto) forward2(T&& t) {
  return std::forward<T>(t);
}
template <typename T>
auto forward3(T&& t) {
  return std::forward<T>(t);
}
template <typename T>
decltype(auto) noforward1(T&& t) {
  return std::decay_t<T>(t);
}
template <typename T>
auto noforward2(T&& t) {
  return t;
}

TEST(Auto, Make) {
  static_assert(std::is_same_v<decltype(make1()), A>, "Type must be equal to A");
  static_assert(std::is_same_v<decltype(make2()), A>, "Type must be equal to A");
  static_assert(std::is_same_v<decltype(make3()), A>, "Type must be equal to A");
}

TEST(Auto, Forward) {
  int v = 2;
  static_assert(std::is_same_v<decltype(forward1(v)), int&>, "Type must be equal to int");
  static_assert(std::is_same_v<decltype(forward2(v)), int&>, "Type must be equal to int");
  static_assert(std::is_same_v<decltype(forward3(v)), int>, "Type must be equal to int");

  static_assert(
      std::is_same_v<decltype(forward1(v + 1)), int&&>, "Type must be equal to int");
  static_assert(
      std::is_same_v<decltype(forward2(v + 1)), int&&>, "Type must be equal to int");
  static_assert(
      std::is_same_v<decltype(forward3(v + 1)), int>, "Type must be equal to int");

  static_assert(std::is_same_v<decltype(forward1(std::move(v))), int&&>,
      "Type must be equal to int");
  static_assert(std::is_same_v<decltype(forward2(std::move(v))), int&&>,
      "Type must be equal to int");
  static_assert(
      std::is_same_v<decltype(forward3(std::move(v))), int>, "Type must be equal to int");
}

TEST(Auto, Noforward) {
  int v = 2;
  static_assert(
      std::is_same_v<decltype(noforward1(v)), int>, "Type must be equal to int");
  static_assert(
      std::is_same_v<decltype(noforward2(v)), int>, "Type must be equal to int");

  static_assert(
      std::is_same_v<decltype(noforward1(v + 1)), int>, "Type must be equal to int");
  static_assert(
      std::is_same_v<decltype(noforward2(v + 1)), int>, "Type must be equal to int");
}

}  // namespace deduction
}  // namespace test