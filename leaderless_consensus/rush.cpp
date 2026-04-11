#include "common.h"

namespace leaderless_consensus::rush {

struct_fields(RushGenerationState, (MessageSeq, messages), (int, generation));
struct_fields(RushPromiseState, (MessageSeq, prefix), (NodeSet, votes));

using RushGenerations = std::vector<RushGenerationState>;
using RushPromises = std::set<RushPromiseState>;

struct_fields(RushCoreState,
    (ProposalSet, proposals),
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
    (ProposalSet, proposed),
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
      for (auto it = result.begin(); it != result.end();) {
        if (it->from == from && it->to == to) {
          it = result.erase(it);
        } else {
          ++it;
        }
      }
      result.insert(RushStateMsg{from, to, core});
    }
  }
  return result;
}

int majority(size_t nodeCount) {
  return static_cast<int>(nodeCount / 2 + 1);
}

int maxGeneration(size_t itemCount) {
  return static_cast<int>(2 * itemCount + 1);
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

NodeSet promiseVotesFor(const RushPromises& promises, const MessageSeq& prefix) {
  for (auto&& promise : promises) {
    if (promise.prefix == prefix) {
      return promise.votes;
    }
  }
  return {};
}

RushPromises putPromiseVotes(RushPromises promises,
    const MessageSeq& prefix,
    const NodeSet& votes) {
  for (auto it = promises.begin(); it != promises.end(); ++it) {
    if (it->prefix == prefix) {
      promises.erase(it);
      break;
    }
  }
  if (!votes.empty()) {
    promises.insert(RushPromiseState{prefix, votes});
  }
  return promises;
}

RushPromises mergePromises(RushPromises left, const RushPromises& right) {
  for (auto&& promise : right) {
    auto votes = setUnion(promiseVotesFor(left, promise.prefix), promise.votes);
    left = putPromiseVotes(std::move(left), promise.prefix, votes);
  }
  return left;
}

RushPromises normalizePromises(const RushPromises& promises,
    const RushGenerations& nodesMessages,
    const ProposalSet& proposals,
    const MessageSeq& committed) {
  RushPromises normalized;
  for (auto&& promise : promises) {
    if (!itemsAreSubset(promise.prefix, proposals) ||
        !allUnique(promise.prefix) ||
        isPrefix(promise.prefix, committed)) {
      continue;
    }
    auto support = prefixSupport(nodesMessages, promise.prefix);
    auto votes = setIntersection(promise.votes, support);
    if (support.empty() || votes.empty()) {
      continue;
    }
    votes = setUnion(promiseVotesFor(normalized, promise.prefix), votes);
    normalized = putPromiseVotes(std::move(normalized), promise.prefix, votes);
  }
  return normalized;
}

bool shouldUseIncoming(const RushGenerationState& current,
    const RushGenerationState& incoming) {
  return incoming.generation > current.generation ||
         (incoming.generation == current.generation &&
             current.messages < incoming.messages);
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
    size_t nodeCount,
    size_t messageCount) {
  auto newCore = makeEmptyCore(nodeCount);
  newCore.nodesMessages = state.core.nodesMessages;

  for (size_t i = 0; i < nodeCount; ++i) {
    if (shouldUseIncoming(newCore.nodesMessages[i], incoming.nodesMessages[i])) {
      newCore.nodesMessages[i] = incoming.nodesMessages[i];
    }
  }

  newCore.proposals = state.core.proposals;
  for (auto&& id : incoming.proposals) {
    if (!newCore.proposals.contains(id)) {
      newCore.proposals.insert(id);
      auto& selfState = newCore.nodesMessages[self];
      selfState.messages.push_back(id);
      selfState.generation = nextGeneration(selfState.generation, messageCount);
    }
  }
  newCore.promises =
      normalizePromises(mergePromises(state.core.promises, incoming.promises),
          newCore.nodesMessages, newCore.proposals, state.committed);

  auto promiseMessages = state.committed;
  auto commitMessages = state.committed;
  auto sorted = false;
  auto i = state.committed.size();
  auto quorum = majority(nodeCount);

  while (i < newCore.proposals.size()) {
    auto id = majorityId(newCore.nodesMessages, i, quorum);
    if (id >= 0) {
      promiseMessages.push_back(id);
      auto support = prefixSupport(newCore.nodesMessages, promiseMessages);
      auto votes = promiseVotesFor(newCore.promises, promiseMessages);
      if (support.contains(self)) {
        votes.insert(self);
      }
      votes = setIntersection(votes, support);
      auto nextCommitted = commitMessages;
      if (static_cast<int>(support.size()) >= quorum &&
          static_cast<int>(votes.size()) >= quorum) {
        nextCommitted = promiseMessages;
      }
      newCore.promises = normalizePromises(
          putPromiseVotes(std::move(newCore.promises), promiseMessages, votes),
          newCore.nodesMessages, newCore.proposals, nextCommitted);
      commitMessages = nextCommitted;
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
              nextGeneration(newCore.nodesMessages[self].generation, messageCount);
          newCore.promises = normalizePromises(
              newCore.promises, newCore.nodesMessages, newCore.proposals, commitMessages);
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

bool canPropose(const RushState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) &&
         !sys.proposed.contains(id) &&
         sys.local.at(node) == RushNodeState{makeEmptyCore(sys.local.size()), {}};
}

RushState propose(RushState sys, NodeId node, MessageId id) {
  sys.proposed.insert(id);
  auto incoming = makeEmptyCore(sys.local.size());
  incoming.proposals.insert(id);
  auto out = mergeState(
      sys.local.at(node), node, incoming, sys.local.size(), sys.proposed.size());
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
  auto out = mergeState(
      sys.local.at(msg.to), msg.to, msg.core, sys.local.size(), sys.proposed.size());
  if (!out.changed) {
    return sys;
  }
  sys.local[msg.to] = RushNodeState{out.core, out.committed};
  sys.stateMsgs = broadcastState(sys.stateMsgs, sys.alive, msg.to, out.core);
  return sys;
}

bool coreWellFormed(const RushCoreState& core,
    const ProposalSet& proposed,
    const NodeSet& allNodes,
    size_t nodeCount) {
  if (!isSubset(core.proposals, proposed) || core.nodesMessages.size() != nodeCount) {
    return false;
  }

  for (auto&& entry : core.nodesMessages) {
    if (entry.generation < 0 ||
        entry.generation > maxGeneration(nodeCount) ||
        !itemsAreSubset(entry.messages, proposed) ||
        !allUnique(entry.messages)) {
      return false;
    }
  }

  for (auto&& promise : core.promises) {
    if (!itemsAreSubset(promise.prefix, proposed) ||
        !allUnique(promise.prefix) ||
        !isSubset(promise.votes, prefixSupport(core.nodesMessages, promise.prefix))) {
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
    if (!coreWellFormed(state.core, sys.proposed, allNodes, sys.local.size()) ||
        !itemsAreSubset(state.committed, sys.proposed) ||
        !allUnique(state.committed)) {
      return false;
    }
  }

  for (auto&& msg : sys.stateMsgs) {
    if (!coreWellFormed(msg.core, sys.proposed, allNodes, sys.local.size())) {
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

bool commitHappened(const RushState& sys) {
  for (auto&& [node, local] : sys.local) {
    if (!local.committed.empty()) {
      return true;
    }
  }
  return false;
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::rush::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::rush::propose)
DEFINE_ALGORITHM(canDeliverStateExpr, ::leaderless_consensus::rush::canDeliverState)
DEFINE_ALGORITHM(deliverStateExpr, ::leaderless_consensus::rush::deliverState)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::rush::invariant)
DEFINE_ALGORITHM(commitHappenedExpr, ::leaderless_consensus::rush::commitHappened)

struct BaseModel : IModel {
  Boolean init() override {
    return sys == makeState(nodes_);
  }

  Boolean proposeAny() {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canProposeExpr(sys, node, id) && sys++ == proposeExpr(sys, node, id);
      };
    };
  }

  Boolean deliverAnyState() {
    return $E(msg, get_mem(sys, stateMsgs)) {
      return canDeliverStateExpr(sys, msg) && sys++ == deliverStateExpr(sys, msg);
    };
  }

  Boolean next() override {
    return proposeAny() || deliverAnyState();
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(sys);
  }

  Var<RushState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = kProposalIds;
};

struct SafetyModel : BaseModel {
  Boolean next() override {
    return proposeAny() || deliverAnyState();
  }
};

struct LivenessModel : BaseModel {
  Boolean next() override {
    return proposeAny() || deliverAnyState();
  }

  std::optional<LivenessBoolean> liveness() override {
    return weakFairness(proposeAny()) &&
           weakFairness(deliverAnyState()) &&
           eventually(commitHappenedExpr(sys));
  }
};

TEST_F(EngineFixture, RushSafetyHoldsInvariant) {
  e.createModel<SafetyModel>();
  EXPECT_NO_THROW(e.run());
}

TEST_F(EngineFixture, RushLivenessCommits) {
  e.createModel<LivenessModel>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::rush
