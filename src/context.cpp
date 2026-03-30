#include "context.h"

State& Context::vars() { return temporalState_[0]; }

State& Context::nexts() { return temporalState_[1]; }

const State& Context::vars() const { return temporalState_[0]; }

const State& Context::nexts() const { return temporalState_[1]; }

size_t Context::size() const { return vars().size(); }

void Context::setState(LogicState state) { state_ = state; }

LogicState Context::getState() const { return state_; }

bool Context::isInit() const { return state_ == LogicState::Init; }

bool Context::isNext() const { return state_ == LogicState::Next; }

std::string Context::toString() const {
  if (isInit()) {
    return asString(vars());
  }
  return asString(vars(), " -> ", nexts());
}

void Context::validate() const { temporalState_.validate(); }

bool Context::isCheck() const { return isCheck_; }

void Context::setCheck(bool is) { isCheck_ = is; }

bool Context::isAddAllowed() const { return isAddAllowed_; }

void Context::setAddAllowed(bool is) { isAddAllowed_ = is; }
