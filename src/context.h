#pragma once

#include "descriptor.h"
#include "error.h"
#include "value.h"

struct Context {
  template <typename T, LogicState I>
  std::optional<T>& getRef(Descriptor& descriptor) {
    ensureRegistered<T, I>(descriptor);
    return temporalState_[static_cast<int>(I)][descriptor.index]
        .template as<T>()
        .getRef();
  }

  State& vars();
  State& nexts();
  const State& vars() const;
  const State& nexts() const;
  size_t size() const;

  void setState(LogicState state);
  LogicState getState() const;
  bool isInit() const;
  bool isNext() const;

  // Expression is used not to assign, but to check (e.g. stop, invariants).
  bool isCheck() const;
  void setCheck(bool is);

  // Whether var registration is allowed. During init, variables must be
  // registered in the first init branch before add is disabled.
  bool isAddAllowed() const;
  void setAddAllowed(bool is);

  std::string toString() const;
  void validate() const;

 private:
  template <typename T, LogicState I>
  void ensureRegistered(Descriptor& descriptor) {
    if constexpr (I == LogicState::Next) {
      if (isInit()) {
        throw VarInitError(asString("Variable ", descriptor.toStringNext(),
                                    " cannot be used during init phase"));
      }
    }
    if (descriptor.isAdded()) {
      return;
    }
    if constexpr (I == LogicState::Next) {
      throw VarInitError(asString("Variable ", descriptor.toStringNext(),
                                  " cannot be used without registration"));
    }
    if (!isAddAllowed_ || !isInit()) {
      throw VarInitError(
          asString("Variable ", descriptor, " must be in init expression"));
    }
    descriptor.index = size();
    vars().push_back(Value::make<T>(descriptor.name));
    nexts().push_back(Value::make<T>(descriptor.name));
  }

  LogicState state_ = LogicState::Init;
  TemporalState temporalState_;
  bool isCheck_ = false;
  bool isAddAllowed_ = true;
};
