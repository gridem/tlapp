#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "boolean.h"
#include "model.h"

struct Stats {
  struct Init {
    size_t vars = 0;
    size_t states = 0;
    std::string toString() const;
  };

  struct Loop {
    size_t states = 0;
    size_t transitions = 0;
    size_t processed = 0;
    std::string toString() const;
  };

  Init init;
  Loop loop;
};

struct Engine {
  // Sets the created model.
  void setModel(Model&& model);

  // Creates the model and returns the reference to created model.
  tname(T, ... V) T& createModel(V&&... v) {
    setModel(std::make_unique<T>(fwd(v)...));
    return static_cast<T&>(*model_);
  }

  // Runs the engine.
  void run();

  // Invokes just init state.
  void init();

  // Iterates though all possible next states.
  void loopNext();

  // Checks liveness obligations over the explored graph.
  void checkLiveness();

  // Returns the current engine stats.
  const Stats& getStats() const;

 private:
  using NodeId = size_t;

  struct GraphInfo {
    std::unordered_map<const State*, NodeId> stateIds;
    std::vector<std::vector<NodeId>> outgoing;
  };

  struct PredicateCache {
    std::vector<uint8_t> holds;
  };

  struct ActionCache {
    std::vector<uint8_t> enabled;
    std::vector<std::vector<NodeId>> targets;
  };

  struct SccInfo {
    std::vector<std::vector<NodeId>> components;
    std::vector<NodeId> componentOf;
    std::vector<uint8_t> infinite;
  };

  // Handles the obtained state.
  void handleResult(const BooleanResult& b, State& to);

  // Adds new state to the hash map and to the queue for subsequent
  // processing.
  void tryAddState(const State& state);

  // Builds indexed adjacency for the admitted graph.
  GraphInfo buildGraphInfo() const;

  // Filters adjacency to the allowed nodes while preserving node ids.
  GraphInfo filterGraphInfo(const GraphInfo& graph,
      const std::vector<uint8_t>& allowed) const;

  // Evaluates predicate on a stored state.
  bool holdsOnState(const BoundPredicate<Boolean>& e, const State& state);

  // Precomputes state-predicate results for all admitted nodes.
  PredicateCache computePredicateCache(const BoundPredicate<Boolean>& e,
      const std::function<void(size_t current, size_t total)>& progress = {});

  // Precomputes enabledness and matching targets for an action on all admitted
  // nodes.
  ActionCache computeActionCache(const BoundNextAction<Boolean>& action,
      const GraphInfo& graph,
      const std::function<void(size_t current, size_t total)>& progress = {});

  // Computes strongly connected components of the admitted graph.
  SccInfo computeSccs(const GraphInfo& graph,
      const std::vector<uint8_t>& allowed = {}) const;

  // Returns true if the SCC admits an infinite behavior.
  bool isInfiniteScc(const std::vector<NodeId>& scc, const GraphInfo& graph) const;

  // Extracts a cycle from the SCC.
  bool extractCycleFromScc(const std::vector<NodeId>& scc,
      const GraphInfo& graph,
      std::vector<const State*>& cycle) const;

  // Checks if there is a cycle (or deadlock-stutter) in the subgraph where
  // the predicate is false.
  bool findEventuallyCounterexample(const PredicateCache& predicate,
      const GraphInfo& graph,
      std::vector<const State*>& cycle) const;

  // Show trace of the current state.
  void trace(const State& state) const;

  // Show cycle of states for liveness failure.
  void traceCycle(const std::vector<const State*>& cycle) const;

  // Returns pointer to stored unique state and whether it was newly inserted.
  std::pair<const State*, bool> tryEmplaceState(const State& state);

  Model model_;
  Context ctx_;

  // Stores one predecessor per discovered state for prefix tracing.
  std::unordered_map<const State*, const State*> processed_;
  // Stores all admitted transitions between states.
  std::unordered_map<const State*, std::vector<const State*>> edges_;
  // Current states to be processed.
  std::deque<const State*> toProcess_;
  // Contains states that survived skip/ensure/stop and belong to the graph.
  std::vector<const State*> graphStates_;
  // Fast membership check for graphStates_.
  std::unordered_set<const State*> admittedStates_;
  // Contains unique states (value-based dedup; pointers remain valid while
  // elements are not erased).
  std::unordered_set<State> uniqueStates_;
  // Current state under consideration from uniqueStates_.
  const State* from_ = nullptr;

  Stats stats_;

  std::optional<BoundPredicate<Boolean>> skip_;
  std::optional<BoundPredicate<Boolean>> ensure_;
  std::optional<BoundPredicate<Boolean>> stop_;
  std::optional<LivenessBoolean> liveness_;
};
