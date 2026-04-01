#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>

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

  // Returns the current engine stats.
  const Stats& getStats() const;

 private:
  // Handles the obtained state.
  void handleResult(const BooleanResult& b, State& to);

  // Adds new state to the hash map and to the queue for subsequent
  // processing.
  void tryAddState(const State& state);

  // Show trace of the current state.
  void trace(const State& state) const;

  // Returns pointer to stored unique state, or nullptr if it already exists.
  const State* tryEmplaceState(const State& state);

  Model model_;
  Context ctx_;

  // Stores all transitions between states (keyed by stable state pointers).
  std::unordered_map<const State*, const State*> processed_;
  // Current states to be processed.
  std::deque<const State*> toProcess_;
  // Contains unique states (value-based dedup; pointers remain valid while
  // elements are not erased).
  std::unordered_set<State> uniqueStates_;
  // Current state under consideration from uniqueStates_.
  const State* from_ = nullptr;

  Stats stats_;

  std::optional<BoundPredicate<Boolean>> skip_;
  std::optional<BoundPredicate<Boolean>> ensure_;
  std::optional<BoundPredicate<Boolean>> stop_;
};
