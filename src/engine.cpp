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

  auto graph = buildGraphInfo();
  auto sccs = computeSccs(graph);

  for (auto&& predicate : liveness_->eventually) {
    auto cache = computePredicateCache(predicate);
    std::vector<const State*> cycle;
    if (!findEventuallyCounterexample(cache, sccs, graph, cycle)) {
      continue;
    }
    LOG(ERROR) << "Liveness eventually condition violated";
    if (!cycle.empty()) {
      trace(*cycle.front());
      traceCycle(cycle);
    }
    throw LivenessError("Eventually condition violated");
  }

  for (auto&& action : liveness_->weakFairness) {
    auto cache = computeActionCache(action, graph);
    std::vector<const State*> cycle;
    if (!findFairnessCounterexample(cache, false, sccs, graph, cycle)) {
      continue;
    }
    LOG(ERROR) << "Liveness weak fairness condition violated";
    if (!cycle.empty()) {
      trace(*cycle.front());
      traceCycle(cycle);
    }
    throw LivenessError("Weak fairness condition violated");
  }

  for (auto&& action : liveness_->strongFairness) {
    auto cache = computeActionCache(action, graph);
    std::vector<const State*> cycle;
    if (!findFairnessCounterexample(cache, true, sccs, graph, cycle)) {
      continue;
    }
    LOG(ERROR) << "Liveness strong fairness condition violated";
    if (!cycle.empty()) {
      trace(*cycle.front());
      traceCycle(cycle);
    }
    throw LivenessError("Strong fairness condition violated");
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

Engine::GraphInfo Engine::buildGraphInfo() const {
  GraphInfo graph;
  graph.stateIds.reserve(graphStates_.size());
  graph.outgoing.resize(graphStates_.size());

  for (size_t i = 0; i < graphStates_.size(); ++i) {
    graph.stateIds.emplace(graphStates_[i], i);
  }

  for (size_t i = 0; i < graphStates_.size(); ++i) {
    auto it = edges_.find(graphStates_[i]);
    if (it == edges_.end()) {
      continue;
    }
    auto& outgoing = graph.outgoing[i];
    outgoing.reserve(it->second.size());
    for (auto&& next : it->second) {
      auto jt = graph.stateIds.find(next);
      if (jt != graph.stateIds.end()) {
        outgoing.push_back(jt->second);
      }
    }
  }

  return graph;
}

bool Engine::holdsOnState(const BoundPredicate<Boolean>& e, const State& state) {
  ctx_.setState(LogicState::Init);
  ctx_.setAddAllowed(false);
  ctx_.setCheck(false);
  ctx_.vars() = state;
  ctx_.nexts().clearValues();
  return e(ctx_);
}

Engine::PredicateCache Engine::computePredicateCache(
    const BoundPredicate<Boolean>& e) {
  PredicateCache cache;
  cache.holds.reserve(graphStates_.size());
  for (auto&& state : graphStates_) {
    cache.holds.push_back(holdsOnState(e, *state) ? 1 : 0);
  }
  return cache;
}

Engine::ActionCache Engine::computeActionCache(
    const BoundNextAction<Boolean>& action, const GraphInfo& graph) {
  ActionCache cache;
  cache.enabled.resize(graphStates_.size());
  cache.targets.resize(graphStates_.size());

  for (size_t i = 0; i < graphStates_.size(); ++i) {
    auto&& outgoing = graph.outgoing[i];
    if (outgoing.empty()) {
      continue;
    }

    std::unordered_set<NodeId> outgoingSet{outgoing.begin(), outgoing.end()};
    std::unordered_set<NodeId> seen;

    ctx_.setState(LogicState::Next);
    ctx_.setAddAllowed(false);
    ctx_.setCheck(false);
    ctx_.vars() = *graphStates_[i];
    ctx_.nexts().clearValues();

    auto bound = action(ctx_);
    std::visit(
        [this, &cache, &graph, &outgoingSet, &seen, i] lam_arg(v) {
          if_eq(v, bool) {
            if (v) {
              throw EngineBooleanError(
                  "Boolean result cannot return true without assignments");
            }
          }
          else {
            for (auto&& assigns : v) {
              if (assigns(ctx_)) {
                auto&& nexts = ctx_.nexts();
                nexts.validate();
                auto jt = uniqueStates_.find(nexts);
                if (jt != uniqueStates_.end()) {
                  auto kt = graph.stateIds.find(&*jt);
                  if (kt != graph.stateIds.end() &&
                      outgoingSet.find(kt->second) != outgoingSet.end() &&
                      seen.insert(kt->second).second) {
                    cache.targets[i].push_back(kt->second);
                  }
                }
              }
              ctx_.nexts().clearValues();
            }
          }
        },
        bound);

    cache.enabled[i] = cache.targets[i].empty() ? 0 : 1;
  }

  return cache;
}

Engine::SccInfo Engine::computeSccs(const GraphInfo& graph) const {
  SccInfo sccs;
  sccs.componentOf.resize(graphStates_.size(), static_cast<NodeId>(-1));

  std::vector<int> index(graphStates_.size(), -1);
  std::vector<int> lowlink(graphStates_.size(), -1);
  std::vector<uint8_t> onStack(graphStates_.size());
  std::vector<NodeId> stack;
  int nextIndex = 0;

  std::function<void(NodeId)> strongConnect = [&](NodeId state) {
    index[state] = nextIndex;
    lowlink[state] = nextIndex;
    ++nextIndex;
    stack.push_back(state);
    onStack[state] = 1;

    for (auto&& next : graph.outgoing[state]) {
      if (index[next] == -1) {
        strongConnect(next);
        lowlink[state] = std::min(lowlink[state], lowlink[next]);
      } else if (onStack[next]) {
        lowlink[state] = std::min(lowlink[state], index[next]);
      }
    }

    if (lowlink[state] != index[state]) {
      return;
    }

    std::vector<NodeId> scc;
    while (true) {
      auto cur = stack.back();
      stack.pop_back();
      onStack[cur] = 0;
      sccs.componentOf[cur] = sccs.components.size();
      scc.push_back(cur);
      if (cur == state) {
        break;
      }
    }
    sccs.components.push_back(std::move(scc));
  };

  for (NodeId state = 0; state < graphStates_.size(); ++state) {
    if (index[state] == -1) {
      strongConnect(state);
    }
  }

  sccs.infinite.reserve(sccs.components.size());
  for (auto&& scc : sccs.components) {
    sccs.infinite.push_back(isInfiniteScc(scc, graph) ? 1 : 0);
  }

  return sccs;
}

bool Engine::isInfiniteScc(const std::vector<NodeId>& scc,
                           const GraphInfo& graph) const {
  if (scc.size() > 1) {
    return true;
  }

  auto state = scc.front();
  auto&& outgoing = graph.outgoing[state];
  if (outgoing.empty()) {
    return true;
  }
  for (auto&& next : outgoing) {
    if (next == state) {
      return true;
    }
  }
  return false;
}

bool Engine::extractCycleFromScc(const std::vector<NodeId>& scc,
                                 const GraphInfo& graph,
                                 std::vector<const State*>& cycle) const {
  if (scc.empty()) {
    return false;
  }

  if (scc.size() == 1) {
    auto state = scc.front();
    auto&& outgoing = graph.outgoing[state];
    if (outgoing.empty()) {
      cycle = {graphStates_[state], graphStates_[state]};
      return true;
    }
    for (auto&& next : outgoing) {
      if (next == state) {
        cycle = {graphStates_[state], graphStates_[state]};
        return true;
      }
    }
    return false;
  }

  std::unordered_set<NodeId> inScc{scc.begin(), scc.end()};
  std::unordered_map<NodeId, int> color;
  std::unordered_map<NodeId, size_t> stackPos;
  std::vector<NodeId> stack;
  std::function<bool(NodeId)> dfs = [&](NodeId state) {
    color[state] = 1;
    stackPos[state] = stack.size();
    stack.push_back(state);

    for (auto&& next : graph.outgoing[state]) {
      if (inScc.find(next) == inScc.end()) {
        continue;
      }
      auto jt = color.find(next);
      if (jt == color.end()) {
        if (dfs(next)) {
          return true;
        }
      } else if (jt->second == 1) {
        auto pos = stackPos[next];
        cycle.clear();
        cycle.reserve(stack.size() - pos + 1);
        for (auto it = stack.begin() + pos; it != stack.end(); ++it) {
          cycle.push_back(graphStates_[*it]);
        }
        cycle.push_back(graphStates_[next]);
        return true;
      }
    }

    color[state] = 2;
    stackPos.erase(state);
    stack.pop_back();
    return false;
  };

  for (auto&& state : scc) {
    if (color.find(state) == color.end() && dfs(state)) {
      return true;
    }
  }
  return false;
}

bool Engine::findEventuallyCounterexample(const PredicateCache& predicate,
                                          const SccInfo& sccs,
                                          const GraphInfo& graph,
                                          std::vector<const State*>& cycle) const {
  for (size_t i = 0; i < sccs.components.size(); ++i) {
    if (!sccs.infinite[i]) {
      continue;
    }

    bool allBad = true;
    for (auto&& node : sccs.components[i]) {
      if (predicate.holds[node]) {
        allBad = false;
        break;
      }
    }

    if (!allBad) {
      continue;
    }

    if (extractCycleFromScc(sccs.components[i], graph, cycle)) {
      return true;
    }
    throw EngineError("Invalid infinite SCC without cycle");
  }
  return false;
}

bool Engine::findFairnessCounterexample(
    const ActionCache& action, bool strong, const SccInfo& sccs,
    const GraphInfo& graph, std::vector<const State*>& cycle) const {
  for (size_t i = 0; i < sccs.components.size(); ++i) {
    if (!sccs.infinite[i]) {
      continue;
    }

    bool anyEnabled = false;
    bool allEnabled = true;
    bool anyTaken = false;

    for (auto&& state : sccs.components[i]) {
      auto enabled = action.enabled[state] != 0;
      anyEnabled = anyEnabled || enabled;
      allEnabled = allEnabled && enabled;

      if (!anyTaken) {
        for (auto&& target : action.targets[state]) {
          if (sccs.componentOf[target] == i) {
            anyTaken = true;
            break;
          }
        }
      }
    }

    auto violated = strong ? (anyEnabled && !anyTaken) : (allEnabled && !anyTaken);
    if (!violated) {
      continue;
    }

    if (extractCycleFromScc(sccs.components[i], graph, cycle)) {
      return true;
    }
    throw EngineError("Invalid infinite SCC without cycle");
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
