#pragma once

#include "as_string.h"
#include "macro.h"
#include "macro_iterator.h"
#include "tag.h"

#define FOR_field_enum(D_field) (D_field, #D_field)

#define FOR_field_less(D_field) \
  if (D_field < o.D_field) {    \
    return true;                \
  }                             \
  if (o.D_field < D_field) {    \
    return false;               \
  }

#define FOR_field_eq(D_field) D_field == o.D_field

/*
#define FOR_field_getter(D_field)                                      \
  template <typename E>                                                \
  static auto get_##D_field(E&& e) {                                   \
    return evaluate(                                                   \
        [](auto&& v) { return std::forward<decltype(v)>(v).D_field; }, \
        std::forward<decltype(e)>(e));                                 \
  }
*/

// Sample: fields(a, b, c)
#define fields(...)                                                   \
  fun(fieldIterator, i) const {                                       \
    i macro_iterate(FOR_field_enum, __VA_ARGS__);                     \
  }                                                                   \
                                                                      \
  tname(T) bool operator<(const T& o) const noexcept {                \
    macro_iterate(FOR_field_less, __VA_ARGS__) return false;          \
  }                                                                   \
                                                                      \
  tname(T) bool operator==(const T& o) const noexcept {               \
    return macro_iterate_delim(FOR_field_eq, macro_and, __VA_ARGS__); \
  }                                                                   \
                                                                      \
  size_t toHash() const {                                             \
    return iteratorResult(HashFieldIterator{}, *this);                \
  }                                                                   \
                                                                      \
  std::string toString() const {                                      \
    return "{" + iteratorResult(StringFieldIterator{}, *this) + "}";  \
  }                                                                   \
                                                                      \
  // macro_iterate(FOR_field_getter, __VA_ARGS__)

#define FOR_ctor_args(D_typeField) \
  macro_index_0 D_typeField macro_index_1 D_typeField = {}

#define FOR_ctor_init(D_typeField) \
  macro_index_1 D_typeField {      \
    macro_index_1 D_typeField      \
  }

#define FOR_struct_fields(D_typeField) \
  macro_index_0 D_typeField macro_index_1 D_typeField;

#define FOR_type_fields(D_typeField) macro_index_1 D_typeField

// Sample: struct_fields(MyStruct, (int, a), (int, b))
#define struct_fields(D_structName, ...)                          \
  struct D_structName : hashable_tag_type {                       \
    D_structName(macro_iterate_comma(FOR_ctor_args, __VA_ARGS__)) \
        : macro_iterate_comma(FOR_ctor_init, __VA_ARGS__) {}      \
    macro_iterate(FOR_struct_fields, __VA_ARGS__)                 \
        fields(macro_iterate_comma(FOR_type_fields, __VA_ARGS__)) \
  };

struct StringFieldIterator {
  fun(operator(), t, name) {
    if (!result_.empty()) {
      result_ += ", ";
    }
    result_ += name;
    result_ += ": ";
    result_ += asString(t);
    return *this;
  }

  std::string result() && {
    return std::move(result_);
  }

 private:
  std::string result_;
};

struct HashFieldIterator {
  fun(operator(), t, name) {
    result_ <<= 1;
    result_ ^= std::hash<std::decay_t<T_t>>{}(t);
    return *this;
  }

  size_t result() const {
    return result_;
  }

 private:
  size_t result_ = 0x4badbed;
};

fun(iteratorResult, iterator, t) {
  t.fieldIterator(iterator);
  return std::move(iterator).result();
}
