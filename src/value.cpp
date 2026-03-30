#include "value.h"

#include "error.h"

Value::Value(const Value& other) : t_{other->clone()} {}

Value::Value(Value&& other) = default;

Value& Value::operator=(const Value& other) {
  if (t_) {
    t_->copyFrom(*other.t_);
  } else {
    t_ = other->clone();
  }
  return *this;
}

Value& Value::operator=(Value&& other) = default;

bool Value::operator==(const Value& other) const {
  return t_->equals(*other.t_);
}

bool Value::operator!=(const Value& other) const { return !operator==(other); }

IValue* Value::operator->() { return t_.get(); }

const IValue* Value::operator->() const { return t_.get(); }

std::string Value::toString() const { return asString(t_->name(), "=", *t_); }

void State::clearValues() {
  for (auto&& s : *this) {
    s->clear();
  }
}

std::string State::toString() const {
  return asString(static_cast<const std::vector<Value>&>(*this));
}

void State::validate() const {
  for (auto&& s : *this) {
    if (s->empty()) {
      throw VarValidationError(
          asString("Variable must be set: ", asStringQuote(s->name())));
    }
  }
}

void TemporalState::validate() const {
  operator[](0).validate();
  for (auto&& s : operator[](1)) {
    if (!s->empty()) {
      throw VarValidationError(asString("Temporal variable cannot be set: ",
                                        asStringQuote(s->name())));
    }
  }
}
