#include "liveness.h"

#include <gtest/gtest.h>

#include "engine_fixture.h"
#include "operation.h"
#include "var.h"

namespace test {

TEST(Liveness, ClauseKindsAndBindingModes) {
  Var<int> x{"x"};

  auto weak = wf(x++ == 3);
  auto strong = sf(x++ == 4);
  auto evt = eventually(x == 2);

  ASSERT_EQ(1, weak.weakFairness.size());
  ASSERT_TRUE(weak.strongFairness.empty());
  ASSERT_TRUE(weak.eventually.empty());

  ASSERT_TRUE(strong.weakFairness.empty());
  ASSERT_EQ(1, strong.strongFairness.size());
  ASSERT_TRUE(strong.eventually.empty());

  ASSERT_TRUE(evt.weakFairness.empty());
  ASSERT_TRUE(evt.strongFairness.empty());
  ASSERT_EQ(1, evt.eventually.size());

  static_assert(
      is_eq<BooleanResult, decltype(weak.weakFairness[0](std::declval<Context&>()))>);
  static_assert(
      is_eq<BooleanResult, decltype(strong.strongFairness[0](std::declval<Context&>()))>);
  static_assert(is_eq<bool, decltype(evt.eventually[0](std::declval<Context&>()))>);
}

TEST(Liveness, ConjunctionMergesClauses) {
  Var<int> x{"x"};
  auto clauses = wf(x++ == 2) && sf(x++ == 3) && eventually(x > 0) && eventually(x != 2);

  ASSERT_EQ(1, clauses.weakFairness.size());
  ASSERT_EQ(1, clauses.strongFairness.size());
  ASSERT_EQ(2, clauses.eventually.size());
}

namespace {

struct EventuallyPassModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x++ == 1;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct EventuallyCycleFailModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x++ == x;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct EventuallyDeadlockFailModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return Boolean{[](Context&) { return BooleanResult{false}; }};
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct EventuallyAfterGoodPrefixPassModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x == 0 && x++ == 1 || x == 1 && x++ == 2 || x == 2 && x++ == 2;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct MultipleEventuallyPassModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x == 0 && x++ == 1 || x == 1 && x++ == 2 || x == 2 && x++ == 2;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1) && eventually(x == 2);
  }

  Var<int> x{"x"};
};

struct MultipleEventuallyFailModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x == 0 && x++ == 1 || x == 1 && x++ == 2 || x == 2 && x++ == 2;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1) && eventually(x == 3);
  }

  Var<int> x{"x"};
};

struct EventuallyMixedInitialFailModel : IModel {
  Boolean init() override {
    return x == 0 || x == 1;
  }

  Boolean next() override {
    return x == 0 && x++ == 0 || x == 1 && x++ == 2 || x == 2 && x++ == 2;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct LivenessWithStopModel : IModel {
  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return x++ == x;
  }

  std::optional<Boolean> stop() override {
    return x == 0;
  }

  std::optional<LivenessBoolean> liveness() override {
    return eventually(x == 1);
  }

  Var<int> x{"x"};
};

struct WeakFairnessChoiceBase : IModel {
  Boolean a() {
    return x == 0 && x++ == 0;
  }

  Boolean b() {
    return x == 0 && x++ == 1;
  }

  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return a() || b();
  }

  Var<int> x{"x"};
};

struct WeakFairnessCombinedModel : WeakFairnessChoiceBase {
  std::optional<LivenessBoolean> liveness() override {
    return wf(a() || b());
  }
};

struct WeakFairnessSeparateModel : WeakFairnessChoiceBase {
  std::optional<LivenessBoolean> liveness() override {
    return wf(a()) && wf(b());
  }
};

struct StrongFairnessChoiceBase : IModel {
  Boolean cycle() {
    return x == 0 && x++ == 1 || x == 1 && x++ == 0;
  }

  Boolean a() {
    return x == 0 && x++ == 2;
  }

  Boolean init() override {
    return x == 0;
  }

  Boolean next() override {
    return cycle() || a();
  }

  Var<int> x{"x"};
};

struct WeakFairnessOnlyModel : StrongFairnessChoiceBase {
  std::optional<LivenessBoolean> liveness() override {
    return wf(a());
  }
};

struct StrongFairnessOnlyModel : StrongFairnessChoiceBase {
  std::optional<LivenessBoolean> liveness() override {
    return sf(a());
  }
};

}  // namespace

TEST_F(EngineFixture, EventuallyPassesWhenAllBehaviorsReachGoal) {
  e.createModel<EventuallyPassModel>();
  ASSERT_NO_THROW(e.run());
}

TEST_F(EngineFixture, EventuallyFailsOnReachableCycle) {
  e.createModel<EventuallyCycleFailModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

TEST_F(EngineFixture, EventuallyFailsOnDeadlockByStuttering) {
  e.createModel<EventuallyDeadlockFailModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

TEST_F(EngineFixture, EventuallyPassesAfterGoodPrefixBeforeBadCycle) {
  e.createModel<EventuallyAfterGoodPrefixPassModel>();
  ASSERT_NO_THROW(e.run());
}

TEST_F(EngineFixture, MultipleEventuallyClausesPass) {
  e.createModel<MultipleEventuallyPassModel>();
  ASSERT_NO_THROW(e.run());
}

TEST_F(EngineFixture, MultipleEventuallyClausesFailWhenOneIsMissing) {
  e.createModel<MultipleEventuallyFailModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

TEST_F(EngineFixture, EventuallyFailsWhenSomeInitialStateCanStayBadForever) {
  e.createModel<EventuallyMixedInitialFailModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

TEST_F(EngineFixture, LivenessRejectsStop) {
  e.createModel<LivenessWithStopModel>();
  ASSERT_THROW(e.init(), EngineError);
}

TEST_F(EngineFixture, WeakFairnessCombinedActionPasses) {
  e.createModel<WeakFairnessCombinedModel>();
  ASSERT_NO_THROW(e.run());
}

TEST_F(EngineFixture, WeakFairnessSeparateActionsFail) {
  e.createModel<WeakFairnessSeparateModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

TEST_F(EngineFixture, WeakFairnessCanPassWhenStrongFairnessFails) {
  e.createModel<WeakFairnessOnlyModel>();
  ASSERT_NO_THROW(e.run());
}

TEST_F(EngineFixture, StrongFairnessFailsWhenActionIsOnlyInfinitelyOftenEnabled) {
  e.createModel<StrongFairnessOnlyModel>();
  ASSERT_THROW(e.run(), LivenessError);
}

}  // namespace test
