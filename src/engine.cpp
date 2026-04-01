#include "engine.h"

#include <functional>
#include <glog/logging.h>

#include "as_string.h"

namespace {

std::optional<BoundPredicate<Boolean>> bindPredicate(
    std::optional<Boolean> e) {
  if (!e) {
    return {};
  }
  return bind(std::move(*e), PredicateMode{});
}

bool holds(const std::optional<BoundPredicate<Boolean>>& e, Context& ctx) {
  return e ? (*e)(ctx) : false;
}

bool notHolds(const std::optional<BoundPredicate<Boolean>>& e, Context& ctx) {
  return e ? !(*e)(ctx) : false;
}

}  // namespace

std::string Stats::Init::toString() const {
  return "{vars: " + asString(vars) + ", initial states: " + asString(states) +
         "}";
}

std::string Stats::Loop::toString() const {
  return "{total states: " + asString(states) +
         ", transitions: " + asString(transitions) + "}";
}

void Engine::setModel(Model&& model) { model_ = std::move(model); }

void Engine::run() {
  init();
  loopNext();
  checkLiveness();
}

void Engine::init() {
  VLOG(1) << "Init started";
  // Get Init expression to initialize init states.
  auto init = bind(model_->init(), InitMode{});

  // Initialize state with types.
  ctx_.setState(LogicState::Init);
  ctx_.setAddAllowed(true);
  ctx_.setCheck(false);
  handleResult(init(ctx_), ctx_.vars());
  stats_.init.vars = ctx_.size();
  stats_.init.states = stats_.loop.states;

  if (stats_.init.vars == 0) {
    throw EngineInitError("Invalid init expression: no assigned variables");
  }
  if (stats_.init.states == 0) {
    throw EngineInitError("Invalid init expression: no possible variants");
  }

  skip_ = bindPredicate(model_->skip());
  ensure_ = bindPredicate(model_->ensure());
  stop_ = bindPredicate(model_->stop());
  liveness_ = model_->liveness();
  if (liveness_) {
    if (stop_) {
      throw EngineError("Liveness cannot be used together with stop()");
    }
    if (!liveness_->weakFairness.empty() || !liveness_->strongFairness.empty()) {
      throw EngineError("WF/SF liveness checking is not implemented yet");
    }
  }

  LOG(INFO) << "Init done: " << asString(stats_.init);
}

void Engine::loopNext() {
  VLOG(1) << "Loop next started";
  auto next = bind(model_->next(), NextMode{});
  auto&& vars = ctx_.vars();
  auto&& nexts = ctx_.nexts();

  ctx_.setState(LogicState::Next);
  ctx_.setAddAllowed(false);
  ctx_.setCheck(false);
  try {
    while (!toProcess_.empty()) {
      from_ = toProcess_.front();
      toProcess_.pop_front();
      vars = *from_;
      VLOG(1) << "Processing state from: " << asString(vars);
      handleResult(next(ctx_), nexts);
      VLOG(1) << "Processing state done";
      LOG_EVERY_N(INFO, 10000) << "Stats: " << asString(stats_.loop);
    }
  } catch (EngineStop&) {
    LOG(INFO) << "Engine stopped";
  }

  LOG(INFO) << "Loop next done: " << asString(stats_.loop);
}

const Stats& Engine::getStats() const { return stats_; }

void Engine::checkLiveness() {
  if (!liveness_) {
    return;
  }

  for (auto&& predicate : liveness_->eventually) {
    std::vector<const State*> cycle;
    if (!findEventuallyCounterexample(predicate, cycle)) {
      continue;
    }
    LOG(ERROR) << "Liveness eventually condition violated";
    if (!cycle.empty()) {
      trace(*cycle.front());
      traceCycle(cycle);
    }
    throw LivenessError("Eventually condition violated");
  }
}

void Engine::handleResult(const BooleanResult& b, State& to) {
  std::visit(
      [this, &to] lam_arg(v) {
        if_eq(v, bool) {
          if (v) {
            throw EngineBooleanError(
                "Boolean result cannot return true without assignments");
          }
          VLOG(1) << "Handle result with false boolean";
        }
        else {
          for (auto&& assigns : v) {
            if (!assigns(ctx_)) {
              VLOG(2) << "Skipping state because of false condition: "
                      << asString(to);
            } else {
              tryAddState(to);
              if (ctx_.isInit() && ctx_.isAddAllowed()) {
                VLOG(2) << "Set allowed flag to false";
                ctx_.setAddAllowed(false);
              }
            }
            to.clearValues();
          }
          VLOG(1) << "Handle result done";
        }
      },
      b);
}

void Engine::tryAddState(const State& state) {
  state.validate();
  ++stats_.loop.transitions;
  auto [stored, inserted] = tryEmplaceState(state);
  if (inserted) {
    if (holds(skip_, ctx_)) {
      VLOG(1) << "Skipping state due to skip expression, state: "
              << asString(ctx_);
      return;
    }
    if (holds(stop_, ctx_)) {
      LOG(INFO) << "Stopping execution due to stop expression, state: "
                << asString(ctx_);
      trace(*stored);
      throw EngineStop{};
    }
    if (notHolds(ensure_, ctx_)) {
      LOG(ERROR) << "Invariant doesn't hold for state: " << asString(ctx_);
      trace(*stored);
      throw EnsureError(asString("Invariant doesn't hold for state: ", ctx_));
    }

    admittedStates_.insert(stored);
    graphStates_.push_back(stored);
    ++stats_.loop.states;
    toProcess_.push_back(stored);
    VLOG(1) << "State inserted: " << asString(ctx_);
  } else if (admittedStates_.find(stored) == admittedStates_.end()) {
    VLOG(1) << "State exists but is not admitted, skipped: " << asString(ctx_);
    return;
  } else {
    VLOG(1) << "State exists, skipped: " << asString(ctx_);
  }

  if (from_ != nullptr && admittedStates_.find(from_) != admittedStates_.end()) {
    edges_[from_].push_back(stored);
  }
}

bool Engine::holdsOnState(const BoundPredicate<Boolean>& e, const State& state) {
  ctx_.setState(LogicState::Init);
  ctx_.setAddAllowed(false);
  ctx_.setCheck(false);
  ctx_.vars() = state;
  ctx_.nexts().clearValues();
  return e(ctx_);
}

bool Engine::findEventuallyCounterexample(const BoundPredicate<Boolean>& e,
                                          std::vector<const State*>& cycle) {
  std::unordered_set<const State*> bad;
  bad.reserve(graphStates_.size());
  for (auto&& state : graphStates_) {
    if (!holdsOnState(e, *state)) {
      bad.insert(state);
    }
  }
  if (bad.empty()) {
    return false;
  }

  std::unordered_map<const State*, int> color;
  std::unordered_map<const State*, size_t> stackPos;
  std::vector<const State*> stack;
  std::function<bool(const State*)> dfs = [&](const State* state) {
    color[state] = 1;
    stackPos[state] = stack.size();
    stack.push_back(state);

    auto it = edges_.find(state);
    if (it == edges_.end() || it->second.empty()) {
      cycle = {state, state};
      return true;
    }

    for (auto&& next : it->second) {
      if (bad.find(next) == bad.end()) {
        continue;
      }
      auto jt = color.find(next);
      if (jt == color.end()) {
        if (dfs(next)) {
          return true;
        }
      } else if (jt->second == 1) {
        auto pos = stackPos[next];
        cycle.assign(stack.begin() + pos, stack.end());
        cycle.push_back(next);
        return true;
      }
    }

    color[state] = 2;
    stackPos.erase(state);
    stack.pop_back();
    return false;
  };

  for (auto&& state : graphStates_) {
    if (bad.find(state) == bad.end()) {
      continue;
    }
    if (color.find(state) != color.end()) {
      continue;
    }
    if (dfs(state)) {
      return true;
    }
  }
  return false;
}

void Engine::trace(const State& state) const {
  std::deque<const State*> seq;
  const State* cur = &state;
  while (true) {
    seq.push_front(cur);
    auto it = processed_.find(cur);
    if (it == processed_.end()) {
      throw EngineError(asString("Invalid trace, cannot find state: ", *cur));
    }
    const State* next = it->second;
    if (next == nullptr) {
      break;
    }
    cur = next;
  }
  for (auto&& s : seq) {
    VLOG(0) << "Trace: " << asString(*s);
  }
}

void Engine::traceCycle(const std::vector<const State*>& cycle) const {
  for (auto&& state : cycle) {
    VLOG(0) << "Cycle: " << asString(*state);
  }
}

std::pair<const State*, bool> Engine::tryEmplaceState(const State& state) {
  auto [it, inserted] = uniqueStates_.insert(state);
  const State* stored = &*it;
  if (inserted) {
    processed_.emplace(stored, from_);
  }
  return {stored, inserted};
}
