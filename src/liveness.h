#pragma once

#include <vector>

#include "boolean.h"

struct LivenessBoolean {
  std::vector<BoundNextAction<Boolean>> weakFairness;
  std::vector<BoundNextAction<Boolean>> strongFairness;
  std::vector<BoundPredicate<Boolean>> eventually;
};

LivenessBoolean weakFairness(Boolean action);
LivenessBoolean strongFairness(Boolean action);
LivenessBoolean eventually(Boolean state);
LivenessBoolean operator&&(LivenessBoolean l, LivenessBoolean r);
