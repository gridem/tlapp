#pragma once

#include <array>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "as_string.h"
#include "hash.h"
#include "object.h"
#include "std.h"

struct IValue : IObject {
  virtual bool equals(const IValue& t) const = 0;
  virtual size_t hash() const = 0;
  virtual std::unique_ptr<IValue> clone() const = 0;
  virtual void copyFrom(const IValue&) = 0;
  virtual const char* name() const = 0;
  virtual std::string toString() const = 0;
  virtual void clear() = 0;
  virtual bool empty() const = 0;

  // Type-erased allocator for the underlying value (currently unused).
  virtual void* newT() const = 0;
};

tname(T) struct TValue : IValue {
  TValue() = default;

  explicit TValue(const char* name) : name_{name} {}

  tname(U) explicit TValue(U&& u, const char* name) : val_{fwd(u)}, name_{name} {}

  bool equals(const IValue& other) const override {
    return val_ == static_cast<const TValue&>(other).val_;
  }

  size_t hash() const override { return calcHash(*val_); }

  std::unique_ptr<IValue> clone() const override {
    return std::make_unique<TValue>(val_, name_);
  }

  void copyFrom(const IValue& other) override {
    val_ = static_cast<const TValue&>(other).get();
  }

  const char* name() const override { return name_; }
  std::string toString() const override { return asString(val_); }

  void clear() override { val_.reset(); }
  bool empty() const override { return !val_.has_value(); }

  std::optional<T>& getRef() { return val_; }
  const std::optional<T>& get() const { return val_; }
  void* newT() const override { return new T{*val_}; }

 private:
  std::optional<T> val_;
  const char* name_ = "";
};

struct Value {
  tname(T, ... V) static Value make(V&&... v) {
    return Value{std::make_unique<TValue<std::decay_t<T>>>(fwd(v)...)};
  }

  Value() = default;
  ~Value() = default;

  explicit Value(const Value& other);
  explicit Value(Value&& other);

  // Intended for tests (currently unused).
  tname(T) explicit Value(const T& val, const char* name) : Value{make<T>(val, name)} {}

  Value& operator=(const Value& other);

  Value& operator=(Value&& other);

  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;

  const IValue* operator->() const;
  IValue* operator->();

  std::string toString() const;

  tname(T) TValue<T>& as() { return static_cast<TValue<T>&>(*t_); }

 private:
  tname(T) explicit Value(std::unique_ptr<TValue<T>>&& t) : t_{std::move(t)} {}

  std::unique_ptr<IValue> t_;
};

struct State : std::vector<Value> {
  using std::vector<Value>::vector;

  void clearValues();

  std::string toString() const;

  void validate() const;
};

// 0 index for current state
// 1 index for next state
struct TemporalState : std::array<State, 2> {
  using std::array<State, 2>::array;

  // Validates that current states are present and temporal states are empty.
  void validate() const;
};

namespace std {

template <>
struct hash<Value> {
  size_t operator()(const Value& t) const noexcept { return t->hash(); }
};

template <>
struct hash<State> : hash<vector<Value>> {
  size_t operator()(const State& s) const noexcept {
    return hash<vector<Value>>::operator()(s);
  }
};

}  // namespace std
