#pragma once

#include "common.h"

namespace leaderless_consensus {

template <typename T_state>
size_t quorumSize(const T_state& state) {
  return state.local.size() / 2 + 1;
}

template <typename T_state>
bool canDisconnect(const T_state& state, NodeId failed) {
  return state.alive.contains(failed);
}

template <typename T_state>
bool canLiveDisconnect(const T_state& state, NodeId failed) {
  return canDisconnect(state, failed) && state.alive.size() - 1 >= quorumSize(state);
}

template <typename T_state, typename T_predicate>
bool anyLocal(const T_state& state, T_predicate&& predicate) {
  for (auto&& [_, local] : state.local) {
    if (predicate(local)) {
      return true;
    }
  }
  return false;
}

template <typename T_state>
bool commitHappenedWithStatus(const T_state& state, int committedStatus) {
  return anyLocal(state, [&](const auto& local) {
    return local.status == committedStatus && !local.committed.empty();
  });
}

template <typename T_state>
bool commitHappenedAnyNonEmpty(const T_state& state) {
  return anyLocal(state, [](const auto& local) { return !local.committed.empty(); });
}

}  // namespace leaderless_consensus
