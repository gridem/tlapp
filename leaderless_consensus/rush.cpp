#include "common.h"

namespace leaderless_consensus::rush {

struct_fields(RushGenerationState, (MessageSeq, messages), (int, generation));
struct_fields(RushPromiseState,
    (MessageSeq, prefix),
    (NodeSet, support),
    (NodeSet, votes));

using RushGenerations = std::vector<RushGenerationState>;
using RushPromises = std::set<RushPromiseState>;

struct_fields(RushCoreState,
    (CarrySet, carries),
    (RushGenerations, nodesMessages),
    (RushPromises, promises));

}  // namespace leaderless_consensus::rush

namespace std {

template <>
struct hash<leaderless_consensus::rush::RushCoreState> {
  size_t operator()(
      const leaderless_consensus::rush::RushCoreState& state) const noexcept {
    return state.toHash();
  }
};

}  // namespace std

namespace leaderless_consensus::rush {

struct_fields(RushNodeState, (RushCoreState, core), (MessageSeq, committed));
struct_fields(RushStateMsg, (int, from), (int, to), (RushCoreState, core));

using RushNodes = std::map<NodeId, RushNodeState>;
using RushStateMessages = std::set<RushStateMsg>;

struct_fields(RushState,
    (NodeSet, alive),
    (CarrySet, applied),
    (RushNodes, local),
    (RushStateMessages, stateMsgs));

RushCoreState makeEmptyCore(size_t nodeCount) {
  return RushCoreState{{}, RushGenerations(nodeCount, RushGenerationState{{}, 0}), {}};
}

RushState makeState(const NodeSet& nodes) {
  auto nodeCount = nodes.size();
  RushNodes local;
  for (auto&& node : nodes) {
    local[node] = RushNodeState{makeEmptyCore(nodeCount), {}};
  }
  return RushState{nodes, {}, local, {}};
}

RushStateMessages broadcastState(const RushStateMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const RushCoreState& core) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(RushStateMsg{from, to, core});
    }
  }
  return result;
}

int majority(size_t nodeCount) {
  return static_cast<int>(nodeCount / 2 + 1);
}

int maxGeneration(size_t /*itemCount*/) {
  return 4;
}

int nextGeneration(int generation, size_t itemCount) {
  return std::min(generation + 1, maxGeneration(itemCount));
}

NodeSet prefixSupport(const RushGenerations& generations, const MessageSeq& prefix) {
  NodeSet support;
  for (size_t node = 0; node < generations.size(); ++node) {
    if (isPrefix(prefix, generations[node].messages)) {
      support.insert(static_cast<NodeId>(node));
    }
  }
  return support;
}

NodeSet promiseVotesFor(const RushPromises& promises,
    const MessageSeq& prefix,
    const NodeSet& support) {
  for (auto&& promise : promises) {
    if (promise.prefix == prefix && promise.support == support) {
      return promise.votes;
    }
  }
  return {};
}

RushPromises putPromiseVotes(RushPromises promises,
    const MessageSeq& prefix,
    const NodeSet& support,
    const NodeSet& votes) {
  for (auto it = promises.begin(); it != promises.end(); ++it) {
    if (it->prefix == prefix && it->support == support) {
      promises.erase(it);
      break;
    }
  }
  promises.insert(RushPromiseState{prefix, support, votes});
  return promises;
}

RushPromises mergePromises(RushPromises left, const RushPromises& right) {
  for (auto&& promise : right) {
    auto votes =
        setUnion(promiseVotesFor(left, promise.prefix, promise.support), promise.votes);
    left = putPromiseVotes(std::move(left), promise.prefix, promise.support, votes);
  }
  return left;
}

MessageId majorityId(const RushGenerations& generations, size_t index, int quorum) {
  std::map<MessageId, int> counts;
  for (auto&& entry : generations) {
    if (index < entry.messages.size()) {
      auto id = entry.messages[index];
      if (++counts[id] >= quorum) {
        return id;
      }
    }
  }
  return -1;
}

struct MergeResult {
  bool changed = false;
  RushCoreState core;
  MessageSeq committed;
};

MergeResult mergeState(const RushNodeState& state,
    NodeId self,
    const RushCoreState& incoming,
    size_t nodeCount) {
  auto newCore = makeEmptyCore(nodeCount);
  newCore.nodesMessages = state.core.nodesMessages;

  for (size_t i = 0; i < nodeCount; ++i) {
    if (incoming.nodesMessages[i].generation > newCore.nodesMessages[i].generation) {
      newCore.nodesMessages[i] = incoming.nodesMessages[i];
    }
  }

  newCore.carries = state.core.carries;
  for (auto&& id : incoming.carries) {
    if (!newCore.carries.contains(id)) {
      newCore.carries.insert(id);
      newCore.nodesMessages[self].messages.push_back(id);
      newCore.nodesMessages[self].generation =
          nextGeneration(state.core.nodesMessages[self].generation, nodeCount);
    }
  }
  newCore.promises = mergePromises(state.core.promises, incoming.promises);

  auto promiseMessages = state.committed;
  auto commitMessages = state.committed;
  auto sorted = false;
  auto i = state.committed.size();
  auto quorum = majority(nodeCount);

  while (i < newCore.carries.size()) {
    auto id = majorityId(newCore.nodesMessages, i, quorum);
    if (id >= 0) {
      promiseMessages.push_back(id);
      auto support = prefixSupport(newCore.nodesMessages, promiseMessages);
      auto votes = promiseVotesFor(newCore.promises, promiseMessages, support);
      if (support.contains(self)) {
        votes.insert(self);
      }
      votes = setIntersection(votes, support);
      newCore.promises =
          putPromiseVotes(std::move(newCore.promises), promiseMessages, support, votes);
      if (static_cast<int>(support.size()) >= quorum &&
          static_cast<int>(votes.size()) >= quorum) {
        commitMessages = promiseMessages;
      }
      ++i;
      continue;
    }

    if (!sorted) {
      sorted = true;
      auto& messages = newCore.nodesMessages[self].messages;
      if (i < messages.size()) {
        auto oldMessages = messages;
        std::sort(messages.begin() + static_cast<std::ptrdiff_t>(i), messages.end());
        if (messages != oldMessages) {
          newCore.nodesMessages[self].generation =
              nextGeneration(state.core.nodesMessages[self].generation, nodeCount);
        }
      }
      continue;
    }

    break;
  }

  if (newCore == state.core) {
    return {false, state.core, state.committed};
  }

  auto committed = state.committed;
  if (commitMessages.size() > committed.size()) {
    committed = commitMessages;
  }
  return {true, newCore, committed};
}

bool canApply(const RushState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) &&
         !sys.applied.contains(id) &&
         sys.local.at(node) == RushNodeState{makeEmptyCore(sys.local.size()), {}};
}

RushState apply(RushState sys, NodeId node, MessageId id) {
  sys.applied.insert(id);
  auto incoming = makeEmptyCore(sys.local.size());
  incoming.carries.insert(id);
  auto out = mergeState(sys.local.at(node), node, incoming, sys.local.size());
  if (!out.changed) {
    return sys;
  }
  sys.local[node] = RushNodeState{out.core, out.committed};
  sys.stateMsgs = broadcastState(sys.stateMsgs, sys.alive, node, out.core);
  return sys;
}

bool canDeliverState(const RushState& sys, const RushStateMsg& msg) {
  return sys.alive.contains(msg.to);
}

RushState deliverState(RushState sys, const RushStateMsg& msg) {
  sys.stateMsgs.erase(msg);
  auto out = mergeState(sys.local.at(msg.to), msg.to, msg.core, sys.local.size());
  if (!out.changed) {
    return sys;
  }
  sys.local[msg.to] = RushNodeState{out.core, out.committed};
  sys.stateMsgs = broadcastState(sys.stateMsgs, sys.alive, msg.to, out.core);
  return sys;
}

bool canDisconnect(const RushState& sys, NodeId failed) {
  return sys.alive.contains(failed);
}

RushState disconnect(RushState sys, NodeId failed) {
  sys.alive.erase(failed);
  sys.stateMsgs = purgeMessages(sys.stateMsgs, failed);
  return sys;
}

bool coreWellFormed(const RushCoreState& core,
    const CarrySet& applied,
    const NodeSet& allNodes,
    size_t nodeCount) {
  if (!isSubset(core.carries, applied) || core.nodesMessages.size() != nodeCount) {
    return false;
  }

  for (auto&& entry : core.nodesMessages) {
    if (entry.generation < 0 ||
        entry.generation > maxGeneration(nodeCount) ||
        !itemsAreSubset(entry.messages, applied) ||
        !allUnique(entry.messages)) {
      return false;
    }
  }

  for (auto&& promise : core.promises) {
    if (!itemsAreSubset(promise.prefix, applied) ||
        !allUnique(promise.prefix) ||
        !isSubset(promise.support, allNodes) ||
        !isSubset(promise.votes, promise.support)) {
      return false;
    }
  }

  return true;
}

bool invariant(const RushState& sys) {
  if (!queueEndpointsAreAlive(sys.stateMsgs, sys.alive)) {
    return false;
  }

  NodeSet allNodes;
  for (auto&& [node, _] : sys.local) {
    allNodes.insert(node);
  }

  for (auto&& [node, state] : sys.local) {
    if (!coreWellFormed(state.core, sys.applied, allNodes, sys.local.size()) ||
        !itemsAreSubset(state.committed, sys.applied) ||
        !allUnique(state.committed)) {
      return false;
    }
  }

  for (auto&& msg : sys.stateMsgs) {
    if (!coreWellFormed(msg.core, sys.applied, allNodes, sys.local.size())) {
      return false;
    }
  }

  for (auto&& [left, leftState] : sys.local) {
    for (auto&& [right, rightState] : sys.local) {
      if (!isPrefix(leftState.committed, rightState.committed) &&
          !isPrefix(rightState.committed, leftState.committed)) {
        return false;
      }
    }
  }

  return true;
}

DEFINE_ALGORITHM(canApplyExpr, ::leaderless_consensus::rush::canApply)
DEFINE_ALGORITHM(applyExpr, ::leaderless_consensus::rush::apply)
DEFINE_ALGORITHM(canDeliverStateExpr, ::leaderless_consensus::rush::canDeliverState)
DEFINE_ALGORITHM(deliverStateExpr, ::leaderless_consensus::rush::deliverState)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::rush::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::rush::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::rush::invariant)

struct Model : IModel {
  Boolean init() override {
    return sys == makeState(nodes_);
  }

  Boolean next() override {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canApplyExpr(sys, node, id) && sys++ == applyExpr(sys, node, id);
      };
    }
    || $E(msg, get_mem(sys, stateMsgs)) {
      return canDeliverStateExpr(sys, msg) && sys++ == deliverStateExpr(sys, msg);
    }
    || $E(failed, nodes_) {
      return canDisconnectExpr(sys, failed) && sys++ == disconnectExpr(sys, failed);
    };
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(sys);
  }

  Var<RushState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  CarrySet messageIds_ = {10, 11, 12};
};

TEST_F(EngineFixture, DISABLED_RushExploration) {
  e.createModel<Model>();
  e.run();
}

}  // namespace leaderless_consensus::rush
