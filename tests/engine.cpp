#include <gtest/gtest.h>

#include "engine_fixture.h"
#include "operation.h"
#include "quantifier.h"
#include "var.h"

namespace test {

namespace {

Boolean nonBooleanCheckResult() {
  return Boolean{[](Context&) {
    return LogicResult::fromRaw([](Context&) { return true; });
  }};
}

}  // namespace

struct Model1 : IModel {
  Boolean init() override { return x == 1 || x == 2; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, Model1) {
  e.createModel<Model1>();
  e.run();
  checkStats(Stats{1, 2, 4, 6});
}

struct Model2 : IModel {
  Boolean init() override { return x == 1 || y == 2; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

TEST_F(EngineFixture, VarInitFailure) {
  e.createModel<Model2>();
  ASSERT_THROW(e.init(), VarInitError);
}

struct Model3 : IModel {
  Boolean init() override { return x == 1 && x == 2; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, InitFalse) {
  e.createModel<Model3>();
  ASSERT_THROW(e.init(), EngineInitError);
}

struct Model4 : IModel {
  Boolean init() override { return x == 1 && x++ == 2; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, InitNext) {
  e.createModel<Model4>();
  ASSERT_THROW(e.init(), VarInitError);
}

struct Model5 : IModel {
  Boolean init() override { return x == 1 && y == 1 || x == 2; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

TEST_F(EngineFixture, InitPartial) {
  e.createModel<Model5>();
  ASSERT_THROW(e.init(), VarValidationError);
}

struct Model6 : IModel {
  Boolean init() override { return x == 1 || x == 2 && y == 1; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

TEST_F(EngineFixture, InitPartial2) {
  e.createModel<Model6>();
  ASSERT_THROW(e.init(), VarInitError);
}

struct Model7 : IModel {
  Boolean init() override { return x == 2 && y == 1; }
  Boolean next() override { return x++ == 5 - x; }

  Var<int> x{"x"};
  Var<int> y{"y"};
};

TEST_F(EngineFixture, NextPartial) {
  e.createModel<Model7>();
  ASSERT_NO_THROW(e.init());
  ASSERT_THROW(e.loopNext(), VarValidationError);
}

struct Model8 : IModel {
  Boolean init() override {
    return $E(v, vals) { return x == v; };
  }
  Boolean next() override { throw 1; }

  Var<int> x{"x"};
  std::set<int> vals = {1, 2};
};

TEST_F(EngineFixture, QuantifierExistsMany) {
  e.createModel<Model8>();
  e.init();
  checkStats(Stats{1, 2, 2, 2});
}

struct Model9 : IModel {
  Boolean init() override { return x == 1; }
  Boolean next() override { return x++ == x + 1; }
  std::optional<Boolean> skip() override { return nonBooleanCheckResult(); }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, SkipRejectsBranchProducingResult) {
  e.createModel<Model9>();
  ASSERT_NO_THROW(e.init());
  ASSERT_THROW(e.loopNext(), EngineBooleanError);
}

struct Model10 : IModel {
  Boolean init() override { return x == 1; }
  Boolean next() override { return x++ == x + 1; }
  std::optional<Boolean> ensure() override { return nonBooleanCheckResult(); }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, EnsureRejectsBranchProducingResult) {
  e.createModel<Model10>();
  ASSERT_NO_THROW(e.init());
  ASSERT_THROW(e.loopNext(), EngineBooleanError);
}

struct Model11 : IModel {
  Boolean init() override { return x == 1; }
  Boolean next() override { return x++ == x + 1; }
  std::optional<Boolean> stop() override { return nonBooleanCheckResult(); }

  Var<int> x{"x"};
};

TEST_F(EngineFixture, StopRejectsBranchProducingResult) {
  e.createModel<Model11>();
  ASSERT_NO_THROW(e.init());
  ASSERT_THROW(e.loopNext(), EngineBooleanError);
}

}  // namespace test
