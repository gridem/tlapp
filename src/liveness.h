#pragma once

#include <vector>

#include "boolean.h"

struct LivenessBoolean {
  std::vector<BoundNextAction<Boolean>> weakFairness;
  std::vector<BoundNextAction<Boolean>> strongFairness;
  std::vector<BoundPredicate<Boolean>> eventually;
};

LivenessBoolean wf(Boolean action);
LivenessBoolean sf(Boolean action);
LivenessBoolean eventually(Boolean state);
LivenessBoolean operator&&(LivenessBoolean l, LivenessBoolean r);
