#include "engine.h"

#include <chrono>
#include <functional>
#include <glog/logging.h>

#include "as_string.h"

namespace {

using BitWord = uint64_t;

struct BitLayout {
  size_t count = 0;
  size_t words = 0;
  BitWord tailMask = 0;
};

std::optional<BoundPredicate<Boolean>> bindPredicate(std::optional<Boolean> e) {
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

BitLayout makeBitLayout(size_t count) {
  auto words = (count + 63) / 64;
  auto tailBits = count % 64;
  auto tailMask =
      words == 0 ? 0 : (tailBits == 0 ? ~BitWord{0} : ((BitWord{1} << tailBits) - 1));
  return {count, words, tailMask};
}

std::vector<BitWord> makeBitRows(size_t rows, const BitLayout& layout) {
  return std::vector<BitWord>(rows * layout.words);
}

BitWord* rowBits(std::vector<BitWord>& bits, const BitLayout& layout, size_t row) {
  return bits.data() + row * layout.words;
}

const BitWord* rowBits(const std::vector<BitWord>& bits,
    const BitLayout& layout,
    size_t row) {
  return bits.data() + row * layout.words;
}

void setBit(std::vector<BitWord>& bits, const BitLayout& layout, size_t row, size_t bit) {
  rowBits(bits, layout, row)[bit / 64] |= BitWord{1} << (bit % 64);
}

void fillFullBits(std::vector<BitWord>& bits, const BitLayout& layout) {
  if (layout.words == 0) {
    return;
  }
  std::fill(bits.begin(), bits.end(), ~BitWord{0});
  bits.back() &= layout.tailMask;
}

bool anyBits(const std::vector<BitWord>& bits) {
  for (auto&& word : bits) {
    if (word != 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::string Stats::Init::toString() const {
  return "{vars: " + asString(vars) + ", initial states: " + asString(states) + "}";
}

std::string Stats::Loop::toString() const {
  auto queued = states >= processed ? states - processed : 0;
  auto drain = states == 0 ? 0 : processed * 100 / states;
  return "{total states: " + asString(states) + ", processed: " + asString(processed) +
         ", queued: " + asString(queued) + ", drain: " + asString(drain) + "%" +
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
  auto lastLog = std::chrono::steady_clock::time_point{};

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
      ++stats_.loop.processed;
      VLOG(1) << "Processing state done";
      auto now = std::chrono::steady_clock::now();
      if (now - lastLog >= std::chrono::seconds(1)) {
        LOG(INFO) << "Stats: " << asString(stats_.loop);
        lastLog = now;
      }
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
  auto weakLayout = makeBitLayout(liveness_->weakFairness.size());
  auto strongLayout = makeBitLayout(liveness_->strongFairness.size());

  for (auto&& predicate : liveness_->eventually) {
    auto cache = computePredicateCache(predicate);
    std::vector<const State*> cycle;
    if (!findEventuallyCounterexample(cache, graph, cycle)) {
      continue;
    }
    LOG(ERROR) << "Liveness eventually condition violated";
    if (!cycle.empty()) {
      trace(*cycle.front());
      traceCycle(cycle);
    }
    throw LivenessError("Eventually condition violated");
  }

  auto weakEnabled = makeBitRows(graphStates_.size(), weakLayout);
  auto weakTaken = makeBitRows(sccs.components.size(), weakLayout);
  for (size_t i = 0; i < liveness_->weakFairness.size(); ++i) {
    auto cache = computeActionCache(liveness_->weakFairness[i], graph);
    for (size_t node = 0; node < cache.enabled.size(); ++node) {
      if (cache.enabled[node]) {
        setBit(weakEnabled, weakLayout, node, i);
      }
      auto component = sccs.componentOf[node];
      for (auto&& target : cache.targets[node]) {
        if (sccs.componentOf[target] == component) {
          setBit(weakTaken, weakLayout, component, i);
          break;
        }
      }
    }
  }

  auto strongEnabled = makeBitRows(graphStates_.size(), strongLayout);
  auto strongTaken = makeBitRows(sccs.components.size(), strongLayout);
  for (size_t i = 0; i < liveness_->strongFairness.size(); ++i) {
    auto cache = computeActionCache(liveness_->strongFairness[i], graph);
    for (size_t node = 0; node < cache.enabled.size(); ++node) {
      if (cache.enabled[node]) {
        setBit(strongEnabled, strongLayout, node, i);
      }
      auto component = sccs.componentOf[node];
      for (auto&& target : cache.targets[node]) {
        if (sccs.componentOf[target] == component) {
          setBit(strongTaken, strongLayout, component, i);
          break;
        }
      }
    }
  }

  if (weakLayout.words != 0) {
    std::vector<BitWord> enabledAll(weakLayout.words);
    std::vector<BitWord> violated(weakLayout.words);
    for (size_t i = 0; i < sccs.components.size(); ++i) {
      if (!sccs.infinite[i]) {
        continue;
      }

      fillFullBits(enabledAll, weakLayout);
      for (auto&& node : sccs.components[i]) {
        auto nodeEnabled = rowBits(weakEnabled, weakLayout, node);
        for (size_t word = 0; word < weakLayout.words; ++word) {
          enabledAll[word] &= nodeEnabled[word];
        }
      }

      auto takenBits = rowBits(weakTaken, weakLayout, i);
      for (size_t word = 0; word < weakLayout.words; ++word) {
        violated[word] = enabledAll[word] & ~takenBits[word];
      }
      violated.back() &= weakLayout.tailMask;
      if (!anyBits(violated)) {
        continue;
      }

      std::vector<const State*> cycle;
      if (!extractCycleFromScc(sccs.components[i], graph, cycle)) {
        throw EngineError("Invalid infinite SCC without cycle");
      }
      LOG(ERROR) << "Liveness weak fairness condition violated";
      if (!cycle.empty()) {
        trace(*cycle.front());
        traceCycle(cycle);
      }
      throw LivenessError("Weak fairness condition violated");
    }
  }

  if (strongLayout.words != 0) {
    std::vector<BitWord> enabledAny(strongLayout.words);
    std::vector<BitWord> violated(strongLayout.words);
    for (size_t i = 0; i < sccs.components.size(); ++i) {
      if (!sccs.infinite[i]) {
        continue;
      }

      std::fill(enabledAny.begin(), enabledAny.end(), 0);
      for (auto&& node : sccs.components[i]) {
        auto nodeEnabled = rowBits(strongEnabled, strongLayout, node);
        for (size_t word = 0; word < strongLayout.words; ++word) {
          enabledAny[word] |= nodeEnabled[word];
        }
      }

      auto takenBits = rowBits(strongTaken, strongLayout, i);
      for (size_t word = 0; word < strongLayout.words; ++word) {
        violated[word] = enabledAny[word] & ~takenBits[word];
      }
      violated.back() &= strongLayout.tailMask;
      if (!anyBits(violated)) {
        continue;
      }

      std::vector<const State*> cycle;
      if (!extractCycleFromScc(sccs.components[i], graph, cycle)) {
        throw EngineError("Invalid infinite SCC without cycle");
      }
      LOG(ERROR) << "Liveness strong fairness condition violated";
      if (!cycle.empty()) {
        trace(*cycle.front());
        traceCycle(cycle);
      }
      throw LivenessError("Strong fairness condition violated");
    }
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
        } else {
          for (auto&& assigns : v) {
            if (!assigns(ctx_)) {
              VLOG(2) << "Skipping state because of false condition: " << asString(to);
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
      VLOG(1) << "Skipping state due to skip expression, state: " << asString(ctx_);
      return;
    }
    if (holds(stop_, ctx_)) {
      LOG(INFO) << "Stopping execution due to stop expression, state: " << asString(ctx_);
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

Engine::PredicateCache Engine::computePredicateCache(const BoundPredicate<Boolean>& e) {
  PredicateCache cache;
  cache.holds.reserve(graphStates_.size());
  for (auto&& state : graphStates_) {
    cache.holds.push_back(holdsOnState(e, *state) ? 1 : 0);
  }
  return cache;
}

Engine::ActionCache Engine::computeActionCache(const BoundNextAction<Boolean>& action,
    const GraphInfo& graph) {
  ActionCache cache;
  cache.enabled.resize(graphStates_.size());
  cache.targets.resize(graphStates_.size());
  // Reuse dense stamp arrays instead of rebuilding per-node hash sets for
  // outgoing membership and target deduplication.
  std::vector<size_t> outgoingMarks(graphStates_.size());
  std::vector<size_t> seenMarks(graphStates_.size());
  size_t outgoingStamp = 1;
  size_t seenStamp = 1;

  for (size_t i = 0; i < graphStates_.size(); ++i) {
    auto&& outgoing = graph.outgoing[i];
    if (outgoing.empty()) {
      continue;
    }

    for (auto&& target : outgoing) {
      outgoingMarks[target] = outgoingStamp;
    }

    auto& targets = cache.targets[i];
    targets.reserve(outgoing.size());

    ctx_.setState(LogicState::Next);
    ctx_.setAddAllowed(false);
    ctx_.setCheck(false);
    ctx_.vars() = *graphStates_[i];
    ctx_.nexts().clearValues();

    auto bound = action(ctx_);
    std::visit(
        [this, &graph, &outgoingMarks, &seenMarks, outgoingStamp, seenStamp,
            &targets] lam_arg(v) {
          if_eq(v, bool) {
            if (v) {
              throw EngineBooleanError(
                  "Boolean result cannot return true without assignments");
            }
          } else {
            for (auto&& assigns : v) {
              if (assigns(ctx_)) {
                auto&& nexts = ctx_.nexts();
                nexts.validate();
                auto jt = uniqueStates_.find(nexts);
                if (jt != uniqueStates_.end()) {
                  auto kt = graph.stateIds.find(&*jt);
                  if (kt != graph.stateIds.end() &&
                      outgoingMarks[kt->second] == outgoingStamp &&
                      seenMarks[kt->second] != seenStamp) {
                    seenMarks[kt->second] = seenStamp;
                    targets.push_back(kt->second);
                  }
                }
              }
              ctx_.nexts().clearValues();
            }
          }
        },
        bound);

    cache.enabled[i] = targets.empty() ? 0 : 1;

    ++outgoingStamp;
    ++seenStamp;
    if (outgoingStamp == 0 || seenStamp == 0) {
      std::fill(outgoingMarks.begin(), outgoingMarks.end(), 0);
      std::fill(seenMarks.begin(), seenMarks.end(), 0);
      outgoingStamp = 1;
      seenStamp = 1;
    }
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

bool Engine::isInfiniteScc(const std::vector<NodeId>& scc, const GraphInfo& graph) const {
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
    const GraphInfo& graph,
    std::vector<const State*>& cycle) const {
  std::vector<uint8_t> color(graphStates_.size());
  std::vector<size_t> stackPos(graphStates_.size());
  std::vector<NodeId> stack;

  std::function<bool(NodeId)> dfs = [&](NodeId state) {
    color[state] = 1;
    stackPos[state] = stack.size();
    stack.push_back(state);

    auto&& outgoing = graph.outgoing[state];
    if (outgoing.empty()) {
      cycle = {graphStates_[state], graphStates_[state]};
      return true;
    }

    for (auto&& next : outgoing) {
      if (predicate.holds[next]) {
        continue;
      }
      if (color[next] == 0) {
        if (dfs(next)) {
          return true;
        }
      } else if (color[next] == 1) {
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
    stack.pop_back();
    return false;
  };

  for (NodeId state = 0; state < graphStates_.size(); ++state) {
    if (predicate.holds[state] || color[state] != 0) {
      continue;
    }
    auto it = processed_.find(graphStates_[state]);
    if (it == processed_.end()) {
      throw EngineError("Invalid trace root for liveness graph");
    }
    if (it->second != nullptr) {
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
