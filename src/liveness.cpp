#include "liveness.h"

#include <iterator>
#include <utility>

namespace {

template <typename T>
void appendVector(std::vector<T>& dst, std::vector<T>& src) {
  if (src.empty()) {
    return;
  }
  if (dst.empty()) {
    dst = std::move(src);
    return;
  }
  dst.insert(dst.end(), std::make_move_iterator(src.begin()),
      std::make_move_iterator(src.end()));
}

}  // namespace

LivenessBoolean wf(Boolean action) {
  return {{bind(std::move(action), NextMode{})}, {}, {}};
}

LivenessBoolean sf(Boolean action) {
  return {{}, {bind(std::move(action), NextMode{})}, {}};
}

LivenessBoolean eventually(Boolean state) {
  return {{}, {}, {bind(std::move(state), PredicateMode{})}};
}

LivenessBoolean operator&&(LivenessBoolean l, LivenessBoolean r) {
  appendVector(l.weakFairness, r.weakFairness);
  appendVector(l.strongFairness, r.strongFairness);
  appendVector(l.eventually, r.eventually);
  return l;
}
