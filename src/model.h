#pragma once

#include "boolean.h"
#include "object.h"

struct IModel : IObject {
  // Initial state.
  virtual Boolean init() = 0;
  // Next state.
  virtual Boolean next() = 0;
  // Skips that state.
  virtual std::optional<Boolean> skip() { return {}; }
  // Ensures state invariant.
  virtual std::optional<Boolean> ensure() { return {}; }
  // Stops on that state.
  virtual std::optional<Boolean> stop() { return {}; }
};

using Model = std::unique_ptr<IModel>;
