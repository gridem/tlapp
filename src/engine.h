#pragma once

#include <deque>
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
  // Handles the obtained state.
  void handleResult(const BooleanResult& b, State& to);

  // Adds new state to the hash map and to the queue for subsequent
  // processing.
  void tryAddState(const State& state);

  // Evaluates predicate on a stored state.
  bool holdsOnState(const BoundPredicate<Boolean>& e, const State& state);

  // Checks if there is a cycle (or deadlock-stutter) in the subgraph where the
  // predicate is false.
  bool findEventuallyCounterexample(const BoundPredicate<Boolean>& e,
                                    std::vector<const State*>& cycle);

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
