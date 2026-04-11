#include "model_common.h"

namespace leaderless_consensus::flat {

constexpr int kVoting = 0;
constexpr int kCommitted = 1;

struct_fields(FlatVoteMsg,
    (int, from),
    (int, to),
    (ProposalSet, proposals),
    (NodeSet, nodes),
    (NodeSet, votes));
struct_fields(FlatNodeState,
    (int, status),
    (NodeSet, nodes),
    (NodeSet, votes),
    (ProposalSet, proposals),
    (ProposalSet, committed));

using FlatNodes = FlatMap<NodeId, FlatNodeState>;
using FlatVoteMessages = FlatSet<FlatVoteMsg>;
using FlatCommitMessages = NodeSet;

struct_fields(FlatState,
    (NodeSet, alive),
    (ProposalSet, proposed),
    (FlatNodes, local),
    (FlatVoteMessages, voteMsgs),
    (FlatCommitMessages, commitMsgs));

FlatVoteMessages broadcastVote(const FlatVoteMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& proposals,
    const NodeSet& nodes,
    const NodeSet& votes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      auto keepNew = true;
      for (auto it = result.begin(); it != result.end();) {
        if (it->from == from &&
            it->to == to &&
            it->proposals == proposals &&
            it->nodes == nodes) {
          if (isSubset(votes, it->votes)) {
            keepNew = false;
            break;
          }
          if (isSubset(it->votes, votes)) {
            it = result.erase(it);
            continue;
          }
        }
        ++it;
      }
      if (keepNew) {
        result.insert(FlatVoteMsg{from, to, proposals, nodes, votes});
      }
    }
  }
  return result;
}

FlatVoteMessages purgeVotesTo(const FlatVoteMessages& messages, NodeId to) {
  FlatVoteMessages result;
  for (auto&& msg : messages) {
    if (msg.to != to) {
      result.insert(msg);
    }
  }
  return result;
}

FlatCommitMessages broadcastCommit(const FlatState& state, NodeId from) {
  auto result = setWithout(state.commitMsgs, from);
  for (auto&& to : state.alive) {
    if (to != from && state.local.at(to).status != kCommitted) {
      result.insert(to);
    }
  }
  return result;
}

FlatState makeState(const NodeSet& nodes) {
  FlatNodes local;
  for (auto&& node : nodes) {
    local[node] = FlatNodeState{kVoting, nodes, {}, {}, {}};
  }
  return FlatState{nodes, {}, local, {}, {}};
}

FlatState commit(FlatState state, NodeId node) {
  auto& self = state.local[node];
  if (self.status == kCommitted) {
    return state;
  }
  self.status = kCommitted;
  self.committed = self.proposals;
  state.voteMsgs = purgeVotesTo(state.voteMsgs, node);
  state.commitMsgs = setWithout(state.commitMsgs, node);
  state.commitMsgs = broadcastCommit(state, node);
  return state;
}

FlatState processVote(FlatState state,
    NodeId node,
    NodeId source,
    const ProposalSet& proposals,
    const NodeSet& incomingNodes,
    const NodeSet& incomingVotes) {
  const auto previous = state.local.at(node);
  if (previous.status == kCommitted || !previous.nodes.contains(source)) {
    return state;
  }

  auto newNodes = setIntersection(incomingNodes, previous.nodes);
  auto newProposals = setUnion(previous.proposals, proposals);
  NodeSet newVotes = {node};

  if (newNodes == incomingNodes && newProposals == proposals) {
    newVotes = setUnion(newVotes, incomingVotes);
  }
  if (newNodes == previous.nodes && newProposals == previous.proposals) {
    newVotes = setUnion(newVotes, previous.votes);
  }
  newVotes = setIntersection(newVotes, newNodes);

  if (newNodes == previous.nodes &&
      newProposals == previous.proposals &&
      newVotes == previous.votes) {
    return state;
  }

  state.local[node] =
      FlatNodeState{kVoting, newNodes, newVotes, newProposals, previous.committed};

  if (newNodes == newVotes) {
    return commit(std::move(state), node);
  }

  state.voteMsgs =
      broadcastVote(state.voteMsgs, state.alive, node, newProposals, newNodes, newVotes);
  return state;
}

bool canPropose(const FlatState& state, NodeId node, MessageId id) {
  return state.alive.contains(node) &&
         !state.proposed.contains(id) &&
         id == node + 10 &&
         state.local.at(node).votes.empty() &&
         state.local.at(node).status != kCommitted;
}

FlatState propose(FlatState state, NodeId node, MessageId id) {
  state.proposed.insert(id);
  auto nodes = state.local.at(node).nodes;
  return processVote(std::move(state), node, node, ProposalSet{id}, nodes, {});
}

bool canDeliverVote(const FlatState& state, const FlatVoteMsg& msg) {
  return state.alive.contains(msg.to);
}

FlatState deliverVote(FlatState state, const FlatVoteMsg& msg) {
  state.voteMsgs.erase(msg);
  return processVote(
      std::move(state), msg.to, msg.from, msg.proposals, msg.nodes, msg.votes);
}

bool canDeliverCommit(const FlatState& state, NodeId node) {
  return state.alive.contains(node) && state.local.at(node).status != kCommitted;
}

FlatState deliverCommit(FlatState state, NodeId node) {
  state.commitMsgs.erase(node);
  return commit(std::move(state), node);
}

FlatState disconnect(FlatState state, NodeId failed) {
  if (!state.alive.contains(failed)) {
    return state;
  }

  state.alive.erase(failed);
  state.voteMsgs = purgeMessages(state.voteMsgs, failed);
  state.commitMsgs.erase(failed);

  auto survivors = state.alive;
  for (auto&& node : survivors) {
    const auto current = state.local.at(node);
    if (current.proposals.empty()) {
      state.local[node].nodes.erase(failed);
      continue;
    }
    state = processVote(std::move(state), node, failed, current.proposals,
        setWithout(current.nodes, failed), {});
  }

  return state;
}

bool invariant(const FlatState& state) {
  if (!queueEndpointsAreAlive(state.voteMsgs, state.alive) ||
      !isSubset(state.commitMsgs, state.alive)) {
    return false;
  }

  for (auto&& node : state.alive) {
    const auto& self = state.local.at(node);
    if (!isSubset(self.proposals, state.proposed) || !isSubset(self.votes, self.nodes)) {
      return false;
    }
    if (self.status == kCommitted) {
      if (self.committed != self.proposals) {
        return false;
      }
    } else if (!self.committed.empty()) {
      return false;
    }
  }

  for (auto&& msg : state.voteMsgs) {
    if (!isSubset(msg.proposals, state.proposed) || !isSubset(msg.votes, msg.nodes)) {
      return false;
    }
  }

  for (auto&& left : state.alive) {
    const auto& committedLeft = state.local.at(left);
    if (committedLeft.status != kCommitted) {
      continue;
    }
    for (auto&& right : state.alive) {
      const auto& committedRight = state.local.at(right);
      if (committedRight.status == kCommitted &&
          committedLeft.committed != committedRight.committed) {
        return false;
      }
    }
  }

  return true;
}

bool commitHappened(const FlatState& state) {
  return commitHappenedWithStatus(state, kCommitted);
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::flat::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::flat::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::flat::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::flat::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::flat::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::flat::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::canDisconnect<FlatState>)
DEFINE_ALGORITHM(canLiveDisconnectExpr,
    ::leaderless_consensus::canLiveDisconnect<FlatState>)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::flat::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::flat::invariant)
DEFINE_ALGORITHM(commitHappenedExpr, ::leaderless_consensus::flat::commitHappened)

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

  Boolean deliverAnyVote() {
    return $E(msg, get_mem(state, voteMsgs)) {
      return canDeliverVoteExpr(state, msg) && state++ == deliverVoteExpr(state, msg);
    };
  }

  Boolean deliverAnyCommit() {
    return $E(node, get_mem(state, commitMsgs)) {
      return canDeliverCommitExpr(state, node) &&
             state++ == deliverCommitExpr(state, node);
    };
  }

  Boolean disconnectAny() {
    return $E(failed, nodes_) {
      return canDisconnectExpr(state, failed) && state++ == disconnectExpr(state, failed);
    };
  }

  Boolean next() override {
    return proposeAny() || deliverAnyVote() || deliverAnyCommit() || disconnectAny();
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(state);
  }

  Var<FlatState> state{"state"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = kProposalIds;
};

struct SafetyModel : BaseModel {
  Boolean disconnectAny() {
    return $E(failed, nodes_) {
      return canDisconnectExpr(state, failed) && state++ == disconnectExpr(state, failed);
    };
  }

  Boolean next() override {
    return proposeAny() || deliverAnyVote() || deliverAnyCommit() || disconnectAny();
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
    return proposeAny() || deliverAnyVote() || deliverAnyCommit() || disconnectAny();
  }

  std::optional<LivenessBoolean> liveness() override {
    return weakFairness(proposeAny()) &&
           weakFairness(deliverAnyVote()) &&
           eventually(commitHappenedExpr(state));
  }
};

TEST_F(EngineFixture, FlatSafetyHoldsInvariant) {
  e.createModel<SafetyModel>();
  EXPECT_NO_THROW(e.run());
}

TEST_F(EngineFixture, FlatLivenessCommitsWithMajorityAlive) {
  e.createModel<LivenessModel>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::flat
