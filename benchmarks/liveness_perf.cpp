#include <gtest/gtest.h>

#include <cstdint>

#include "bench_util.h"
#include "engine.h"
#include "operation.h"
#include "var.h"

namespace liveness_perf {
namespace {

Boolean makeRingStep(Var<int>& node, int size) {
  Boolean expr = node == 0 && node++ == (size == 1 ? 0 : 1);
  for (int i = 1; i + 1 < size; ++i) {
    expr = expr || (node == i && node++ == i + 1);
  }
  if (size > 1) {
    expr = expr || (node == size - 1 && node++ == 0);
  }
  return expr;
}

Boolean makeGuardedRingStep(Var<int>& node, Var<int>& done, int size) {
  Boolean expr = done == 0 && node == 0 && node++ == (size == 1 ? 0 : 1) && done++ == 0;
  for (int i = 1; i + 1 < size; ++i) {
    expr = expr || (done == 0 && node == i && node++ == i + 1 && done++ == 0);
  }
  if (size > 1) {
    expr = expr || (done == 0 && node == size - 1 && node++ == 0 && done++ == 0);
  }
  return expr;
}

Boolean makeExitAll(Var<int>& node, Var<int>& done, int size) {
  Boolean expr = done == 0 && node == 0 && node++ == -1 && done++ == 1;
  for (int i = 1; i < size; ++i) {
    expr = expr || (done == 0 && node == i && node++ == -1 && done++ == 1);
  }
  return expr;
}

Boolean makeSinkStep(Var<int>& node, Var<int>& done) {
  return done == 1 && node == -1 && node++ == -1 && done++ == 1;
}

struct EventuallyRingModel : IModel {
  explicit EventuallyRingModel(int size)
      : size_{size},
        goal_{size / 2},
        nextExpr_{makeRingStep(node, size_)},
        livenessExpr_{eventually(node == goal_)} {}

  Boolean init() override {
    return node == 0;
  }

  Boolean next() override {
    return nextExpr_;
  }

  std::optional<LivenessBoolean> liveness() override {
    return livenessExpr_;
  }

  Var<int> node{"node"};
  int size_;
  int goal_;
  Boolean nextExpr_;
  std::optional<LivenessBoolean> livenessExpr_;
};

struct FairnessCycleBase : IModel {
  explicit FairnessCycleBase(int size)
      : size_{size},
        cycleExpr_{makeGuardedRingStep(node, done, size_)},
        exitExpr_{makeExitAll(node, done, size_)},
        sinkExpr_{makeSinkStep(node, done)},
        nextExpr_{cycleExpr_ || exitExpr_ || sinkExpr_} {}

  Boolean init() override {
    return node == 0 && done == 0;
  }

  Boolean next() override {
    return nextExpr_;
  }

  Var<int> node{"node"};
  Var<int> done{"done"};
  int size_;
  Boolean cycleExpr_;
  Boolean exitExpr_;
  Boolean sinkExpr_;
  Boolean nextExpr_;
};

struct WeakFairnessCycleModel : FairnessCycleBase {
  explicit WeakFairnessCycleModel(int size)
      : FairnessCycleBase(size), livenessExpr_{wf(cycleExpr_)} {}

  std::optional<LivenessBoolean> liveness() override {
    return livenessExpr_;
  }

  std::optional<LivenessBoolean> livenessExpr_;
};

struct StrongFairnessCycleModel : FairnessCycleBase {
  explicit StrongFairnessCycleModel(int size)
      : FairnessCycleBase(size), livenessExpr_{sf(cycleExpr_)} {}

  std::optional<LivenessBoolean> liveness() override {
    return livenessExpr_;
  }

  std::optional<LivenessBoolean> livenessExpr_;
};

template <typename T_model, typename... T_args>
Engine makeExploredEngine(T_args&&... args) {
  Engine e;
  e.createModel<T_model>(fwd(args)...);
  e.init();
  e.loopNext();
  return e;
}

}  // namespace

TEST(LivenessPerf, Run) {
  constexpr int kEventuallySize = 4096;
  constexpr int kFairnessSize = 1024;

  auto eventuallyEngine = makeExploredEngine<EventuallyRingModel>(kEventuallySize);
  ASSERT_EQ(
      static_cast<size_t>(kEventuallySize), eventuallyEngine.getStats().loop.states);
  ASSERT_NO_THROW(eventuallyEngine.checkLiveness());
  auto eventuallyToken = benchValue(eventuallyEngine.getStats());
  expectBenchPerIteration("liveness_eventually_ring_4096", 200, eventuallyToken, [&]() {
    eventuallyEngine.checkLiveness();
    return eventuallyToken;
  });

  auto weakFairnessEngine = makeExploredEngine<WeakFairnessCycleModel>(kFairnessSize);
  ASSERT_EQ(
      static_cast<size_t>(kFairnessSize + 1), weakFairnessEngine.getStats().loop.states);
  ASSERT_NO_THROW(weakFairnessEngine.checkLiveness());
  auto weakFairnessToken = benchValue(weakFairnessEngine.getStats());
  expectBenchPerIteration("liveness_wf_cycle_1024", 25, weakFairnessToken, [&]() {
    weakFairnessEngine.checkLiveness();
    return weakFairnessToken;
  });

  auto strongFairnessEngine = makeExploredEngine<StrongFairnessCycleModel>(kFairnessSize);
  ASSERT_EQ(static_cast<size_t>(kFairnessSize + 1),
      strongFairnessEngine.getStats().loop.states);
  ASSERT_NO_THROW(strongFairnessEngine.checkLiveness());
  auto strongFairnessToken = benchValue(strongFairnessEngine.getStats());
  expectBenchPerIteration("liveness_sf_cycle_1024", 25, strongFairnessToken, [&]() {
    strongFairnessEngine.checkLiveness();
    return strongFairnessToken;
  });
}

}  // namespace liveness_perf
