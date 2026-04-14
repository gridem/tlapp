#include "model_common.h"

namespace leaderless_consensus::rush {

struct_fields(RushGenerationState, (MessageSeq, messages), (int, generation));
struct_fields(RushPromiseState, (MessageSeq, prefix), (NodeSet, votes));

using RushGenerations = std::vector<RushGenerationState>;
using RushPromises = FlatSet<RushPromiseState>;

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
struct_fields(RushStateMsg,
    (int, from),
    (int, to),
    (RushCoreState, core),
    (MessageSeq, committed));

using RushNodes = FlatMap<NodeId, RushNodeState>;
using RushStateMessages = FlatSet<RushStateMsg>;

struct_fields(RushState,
    (NodeSet, alive),
    (ProposalSet, proposed),
    (RushNodes, local),
    (RushStateMessages, stateMsgs));

RushGenerationState emptyGenerationState() {
  return RushGenerationState{{}, 0};
}

RushCoreState makeEmptyCore(size_t nodeCount) {
  return RushCoreState{{}, RushGenerations(nodeCount, emptyGenerationState()), {}};
}

RushNodeState makeEmptyNodeState(size_t nodeCount) {
  return RushNodeState{makeEmptyCore(nodeCount), {}};
}

RushState makeState(const NodeSet& nodes) {
  auto nodeCount = nodes.size();
  RushNodes local;
  for (auto&& node : nodes) {
    local[node] = makeEmptyNodeState(nodeCount);
  }
  return RushState{nodes, {}, local, {}};
}

MessageSeq concatMessages(const MessageSeq& left, const MessageSeq& right) {
  MessageSeq result = left;
  for (auto&& id : right) {
    result.push_back(id);
  }
  return result;
}

ProposalSet messageSet(const MessageSeq& messages) {
  ProposalSet result;
  for (auto&& id : messages) {
    result.insert(id);
  }
  return result;
}

ProposalSet withoutMessages(ProposalSet proposals, const MessageSeq& messages) {
  for (auto&& id : messages) {
    proposals.erase(id);
  }
  return proposals;
}

MessageSeq suffixAfter(const MessageSeq& values, const MessageSeq& prefix) {
  if (!isPrefix(prefix, values)) {
    return {};
  }
  MessageSeq result;
  for (size_t i = prefix.size(); i < values.size(); ++i) {
    result.push_back(values[i]);
  }
  return result;
}

MessageSeq mergeCommitted(const MessageSeq& left, const MessageSeq& right) {
  if (isPrefix(left, right)) {
    return right;
  }
  if (isPrefix(right, left)) {
    return left;
  }
  return left;
}

MessageSeq withoutCommitted(const MessageSeq& messages, const MessageSeq& committed) {
  auto committedIds = messageSet(committed);
  MessageSeq result;
  for (auto&& id : messages) {
    if (!committedIds.contains(id)) {
      result.push_back(id);
    }
  }
  return result;
}

RushStateMessages broadcastState(const RushStateMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const RushNodeState& state) {
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
      result.insert(RushStateMsg{from, to, state.core, state.committed});
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

NodeSet prefixSupport(const RushGenerations& generations,
    const NodeSet& alive,
    const MessageSeq& prefix) {
  NodeSet support;
  for (auto&& node : alive) {
    if (isPrefix(prefix, generations[node].messages)) {
      support.insert(node);
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
  if (!votes.empty() && !prefix.empty()) {
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
    const NodeSet& alive,
    const ProposalSet& proposals) {
  RushPromises normalized;
  for (auto&& promise : promises) {
    if (promise.prefix.empty() ||
        !itemsAreSubset(promise.prefix, proposals) ||
        !allUnique(promise.prefix)) {
      continue;
    }
    auto support = prefixSupport(nodesMessages, alive, promise.prefix);
    auto votes = setIntersection(promise.votes, support);
    if (support.empty() || votes.empty()) {
      continue;
    }
    votes = setUnion(promiseVotesFor(normalized, promise.prefix), votes);
    normalized = putPromiseVotes(std::move(normalized), promise.prefix, votes);
  }
  return normalized;
}

RushGenerationState rebaseGeneration(const RushGenerationState& state,
    const MessageSeq& fromCommitted,
    const MessageSeq& toCommitted) {
  auto delta = suffixAfter(toCommitted, fromCommitted);
  auto messages = state.messages;
  if (!delta.empty() && isPrefix(delta, messages)) {
    messages = suffixAfter(messages, delta);
  }
  messages = withoutCommitted(messages, toCommitted);
  return RushGenerationState{messages, state.generation};
}

RushPromises rebasePromises(const RushPromises& promises,
    const MessageSeq& fromCommitted,
    const MessageSeq& toCommitted) {
  auto delta = suffixAfter(toCommitted, fromCommitted);
  RushPromises rebased;
  for (auto&& promise : promises) {
    auto prefix = promise.prefix;
    if (!delta.empty() && isPrefix(delta, prefix)) {
      prefix = suffixAfter(prefix, delta);
    }
    prefix = withoutCommitted(prefix, toCommitted);
    if (prefix.empty()) {
      continue;
    }
    auto votes = setUnion(promiseVotesFor(rebased, prefix), promise.votes);
    rebased = putPromiseVotes(std::move(rebased), prefix, votes);
  }
  return rebased;
}

RushCoreState rebaseCore(const RushCoreState& core,
    const NodeSet& alive,
    const MessageSeq& fromCommitted,
    const MessageSeq& toCommitted) {
  RushGenerations nodesMessages(core.nodesMessages.size(), emptyGenerationState());
  for (size_t i = 0; i < core.nodesMessages.size(); ++i) {
    nodesMessages[i] =
        rebaseGeneration(core.nodesMessages[i], fromCommitted, toCommitted);
  }
  auto proposals = withoutMessages(core.proposals, toCommitted);
  auto promises =
      normalizePromises(rebasePromises(core.promises, fromCommitted, toCommitted),
          nodesMessages, alive, proposals);
  return RushCoreState{proposals, nodesMessages, promises};
}

bool shouldUseIncoming(const RushGenerationState& current,
    const RushGenerationState& incoming) {
  return incoming.generation > current.generation ||
         (incoming.generation == current.generation &&
             current.messages < incoming.messages);
}

MessageId majorityId(const RushGenerations& generations,
    const NodeSet& alive,
    size_t index,
    int quorum) {
  FlatMap<MessageId, int> counts;
  for (auto&& node : alive) {
    auto&& entry = generations[node];
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
  RushNodeState local;
};

MergeResult mergeState(const RushNodeState& state,
    NodeId self,
    const RushNodeState& incoming,
    const NodeSet& alive,
    size_t nodeCount,
    size_t messageCount) {
  auto committed = mergeCommitted(state.committed, incoming.committed);
  auto currentCore = rebaseCore(state.core, alive, state.committed, committed);
  auto incomingCore = rebaseCore(incoming.core, alive, incoming.committed, committed);

  auto newCore = makeEmptyCore(nodeCount);
  newCore.nodesMessages = currentCore.nodesMessages;
  for (size_t i = 0; i < nodeCount; ++i) {
    if (shouldUseIncoming(newCore.nodesMessages[i], incomingCore.nodesMessages[i])) {
      newCore.nodesMessages[i] = incomingCore.nodesMessages[i];
    }
  }

  newCore.proposals = currentCore.proposals;
  for (auto&& id : incomingCore.proposals) {
    if (!newCore.proposals.contains(id)) {
      newCore.proposals.insert(id);
      auto& selfState = newCore.nodesMessages[self];
      selfState.messages.push_back(id);
      selfState.generation = nextGeneration(selfState.generation, messageCount);
    }
  }
  newCore.promises =
      normalizePromises(mergePromises(currentCore.promises, incomingCore.promises),
          newCore.nodesMessages, alive, newCore.proposals);

  auto promiseMessages = MessageSeq{};
  auto commitMessages = MessageSeq{};
  auto sorted = false;
  auto i = size_t{0};
  auto quorum = majority(alive.size());

  while (i < newCore.proposals.size()) {
    auto id = majorityId(newCore.nodesMessages, alive, i, quorum);
    if (id >= 0) {
      promiseMessages.push_back(id);
      auto support = prefixSupport(newCore.nodesMessages, alive, promiseMessages);
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
          newCore.nodesMessages, alive, newCore.proposals);
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
              newCore.promises, newCore.nodesMessages, alive, newCore.proposals);
        }
      }
      continue;
    }

    break;
  }

  auto finalCommitted = concatMessages(committed, commitMessages);
  auto finalCore = rebaseCore(newCore, alive, committed, finalCommitted);
  auto local = RushNodeState{finalCore, finalCommitted};
  return {local != state, local};
}

bool canPropose(const RushState& state, NodeId node, MessageId id) {
  return state.alive.contains(node) &&
         !state.proposed.contains(id) &&
         state.local.at(node) == makeEmptyNodeState(state.local.size());
}

RushState propose(RushState state, NodeId node, MessageId id) {
  state.proposed.insert(id);
  auto incoming = makeEmptyNodeState(state.local.size());
  incoming.core.proposals.insert(id);
  auto out = mergeState(state.local.at(node), node, incoming, state.alive,
      state.local.size(), state.proposed.size());
  if (!out.changed) {
    return state;
  }
  state.local[node] = out.local;
  state.stateMsgs = broadcastState(state.stateMsgs, state.alive, node, out.local);
  return state;
}

bool canDeliverState(const RushState& state, const RushStateMsg& msg) {
  return state.alive.contains(msg.to) &&
         (isPrefix(state.local.at(msg.to).committed, msg.committed) ||
             isPrefix(msg.committed, state.local.at(msg.to).committed));
}

RushState deliverState(RushState state, const RushStateMsg& msg) {
  state.stateMsgs.erase(msg);
  auto incoming = RushNodeState{msg.core, msg.committed};
  auto out = mergeState(state.local.at(msg.to), msg.to, incoming, state.alive,
      state.local.size(), state.proposed.size());
  if (!out.changed) {
    return state;
  }
  state.local[msg.to] = out.local;
  state.stateMsgs = broadcastState(state.stateMsgs, state.alive, msg.to, out.local);
  return state;
}

RushCoreState pruneFailedCore(RushCoreState core, const NodeSet& alive, NodeId failed) {
  core.nodesMessages[failed] = emptyGenerationState();
  core.promises =
      normalizePromises(core.promises, core.nodesMessages, alive, core.proposals);
  return core;
}

RushState disconnect(RushState state, NodeId failed) {
  if (!state.alive.contains(failed)) {
    return state;
  }

  state.alive.erase(failed);
  RushStateMessages prunedMessages;
  for (auto&& msg : state.stateMsgs) {
    if (msg.from == failed || msg.to == failed) {
      continue;
    }
    prunedMessages.insert(RushStateMsg{
        msg.from, msg.to, pruneFailedCore(msg.core, state.alive, failed), msg.committed});
  }
  state.stateMsgs = prunedMessages;

  auto survivors = state.alive;
  for (auto&& node : survivors) {
    const auto current = state.local.at(node);
    auto next = current;
    next.core = pruneFailedCore(next.core, state.alive, failed);
    auto out = mergeState(next, node,
        RushNodeState{makeEmptyCore(state.local.size()), next.committed}, state.alive,
        state.local.size(), state.proposed.size());
    auto effective = out.local;
    if (effective == current) {
      continue;
    }
    state.local[node] = effective;
    state.stateMsgs = broadcastState(state.stateMsgs, state.alive, node, effective);
  }

  return state;
}

bool canStabilize(const RushState& state, NodeId node) {
  if (!state.alive.contains(node)) {
    return false;
  }
  auto current = state.local.at(node);
  auto out = mergeState(current, node,
      RushNodeState{makeEmptyCore(state.local.size()), current.committed}, state.alive,
      state.local.size(), state.proposed.size());
  return out.changed;
}

RushState stabilize(RushState state, NodeId node) {
  auto current = state.local.at(node);
  auto out = mergeState(current, node,
      RushNodeState{makeEmptyCore(state.local.size()), current.committed}, state.alive,
      state.local.size(), state.proposed.size());
  if (!out.changed) {
    return state;
  }
  state.local[node] = out.local;
  state.stateMsgs = broadcastState(state.stateMsgs, state.alive, node, out.local);
  return state;
}

bool coreWellFormed(const RushCoreState& core,
    const NodeSet& alive,
    const MessageSeq& committed,
    const ProposalSet& proposed,
    size_t nodeCount) {
  auto committedIds = messageSet(committed);
  if (!isSubset(core.proposals, proposed) ||
      !setIntersection(core.proposals, committedIds).empty() ||
      core.nodesMessages.size() != nodeCount) {
    return false;
  }

  for (auto&& entry : core.nodesMessages) {
    if (entry.generation < 0 ||
        entry.generation > maxGeneration(nodeCount) ||
        !itemsAreSubset(entry.messages, core.proposals) ||
        !allUnique(entry.messages)) {
      return false;
    }
  }

  for (auto&& promise : core.promises) {
    if (promise.prefix.empty() ||
        !itemsAreSubset(promise.prefix, core.proposals) ||
        !allUnique(promise.prefix) ||
        !isSubset(
            promise.votes, prefixSupport(core.nodesMessages, alive, promise.prefix))) {
      return false;
    }
  }

  return true;
}

bool invariant(const RushState& state) {
  if (!queueEndpointsAreAlive(state.stateMsgs, state.alive)) {
    return false;
  }

  for (auto&& node : state.alive) {
    auto&& local = state.local.at(node);
    if (!coreWellFormed(local.core, state.alive, local.committed, state.proposed,
            state.local.size()) ||
        !itemsAreSubset(local.committed, state.proposed) ||
        !allUnique(local.committed)) {
      return false;
    }
  }

  for (auto&& msg : state.stateMsgs) {
    if (!coreWellFormed(
            msg.core, state.alive, msg.committed, state.proposed, state.local.size()) ||
        !itemsAreSubset(msg.committed, state.proposed) ||
        !allUnique(msg.committed)) {
      return false;
    }
  }

  for (auto&& left : state.alive) {
    const auto& leftState = state.local.at(left);
    for (auto&& right : state.alive) {
      const auto& rightState = state.local.at(right);
      if (!isPrefix(leftState.committed, rightState.committed) &&
          !isPrefix(rightState.committed, leftState.committed)) {
        return false;
      }
    }
  }

  return true;
}

bool commitHappened(const RushState& state) {
  if (state.alive.empty()) {
    return false;
  }
  for (auto&& node : state.alive) {
    if (state.local.at(node).committed.empty()) {
      return false;
    }
  }
  return true;
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::rush::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::rush::propose)
DEFINE_ALGORITHM(canDeliverStateExpr, ::leaderless_consensus::rush::canDeliverState)
DEFINE_ALGORITHM(deliverStateExpr, ::leaderless_consensus::rush::deliverState)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::canDisconnect<RushState>)
DEFINE_ALGORITHM(canLiveDisconnectExpr,
    ::leaderless_consensus::canLiveDisconnect<RushState>)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::rush::disconnect)
DEFINE_ALGORITHM(canStabilizeExpr, ::leaderless_consensus::rush::canStabilize)
DEFINE_ALGORITHM(stabilizeExpr, ::leaderless_consensus::rush::stabilize)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::rush::invariant)
DEFINE_ALGORITHM(commitHappenedExpr, ::leaderless_consensus::rush::commitHappened)

struct BaseModel : IModel {
  Boolean init() override {
    return state == makeState(nodes_);
  }

  Boolean proposeAny() {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canProposeExpr(state, node, id) && state++ == proposeExpr(state, node, id);
      };
    };
  }

  Boolean deliverAnyState() {
    return $E(msg, get_mem(state, stateMsgs)) {
      return canDeliverStateExpr(state, msg) && state++ == deliverStateExpr(state, msg);
    };
  }

  Boolean stabilizeAny() {
    return $E(node, nodes_) {
      return canStabilizeExpr(state, node) && state++ == stabilizeExpr(state, node);
    };
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(state);
  }

  Var<RushState> state{"state"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = kProposalIds;
};

struct SafetyModel : BaseModel {
  Boolean next() override {
    return proposeAny() || deliverAnyState() || stabilizeAny();
  }
};

struct LivenessModel : BaseModel {
  Boolean disconnectAny() {
    return $E(failed, nodes_) {
      return canLiveDisconnectExpr(state, failed) &&
             state++ == disconnectExpr(state, failed);
    };
  }

  Boolean next() override {
    return proposeAny() || deliverAnyState() || stabilizeAny() || disconnectAny();
  }

  std::optional<LivenessBoolean> liveness() override {
    return weakFairness(proposeAny()) &&
           weakFairness(deliverAnyState()) &&
           weakFairness(stabilizeAny()) &&
           eventually(commitHappenedExpr(state));
  }
};

TEST_F(EngineFixture, RushSafetyHoldsInvariant) {
  e.createModel<SafetyModel>();
  EXPECT_NO_THROW(e.run());
}

TEST_F(EngineFixture, RushLivenessAllAliveCommitWithMajorityAlive) {
  e.createModel<LivenessModel>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::rush
