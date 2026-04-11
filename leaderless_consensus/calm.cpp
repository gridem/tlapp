#include "common.h"

namespace leaderless_consensus::calm {

constexpr int kToVote = 0;
constexpr int kMayCommit = 1;
constexpr int kCannotCommit = 2;
constexpr int kCompleted = 3;

struct_fields(CalmVoteMsg,
    (int, from),
    (int, to),
    (ProposalSet, proposals),
    (NodeSet, nodes));
struct_fields(CalmCommitMsg, (int, from), (int, to), (ProposalSet, commit));
struct_fields(CalmNodeState,
    (int, status),
    (NodeSet, nodes),
    (NodeSet, voted),
    (ProposalSet, proposals),
    (ProposalSet, committed));

using CalmNodes = std::map<NodeId, CalmNodeState>;
using CalmVoteMessages = std::set<CalmVoteMsg>;
using CalmCommitMessages = std::set<CalmCommitMsg>;

struct_fields(CalmState,
    (NodeSet, alive),
    (ProposalSet, proposed),
    (CalmNodes, local),
    (CalmVoteMessages, voteMsgs),
    (CalmCommitMessages, commitMsgs));

CalmVoteMessages broadcastVote(const CalmVoteMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& proposals,
    const NodeSet& nodes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(CalmVoteMsg{from, to, proposals, nodes});
    }
  }
  return result;
}

CalmCommitMessages broadcastCommit(const CalmCommitMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& commit) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(CalmCommitMsg{from, to, commit});
    }
  }
  return result;
}

CalmState makeState(const NodeSet& nodes) {
  CalmNodes local;
  for (auto&& node : nodes) {
    local[node] = CalmNodeState{kToVote, nodes, {}, {}, {}};
  }
  return CalmState{nodes, {}, local, {}, {}};
}

CalmState commit(CalmState state, NodeId node, const ProposalSet& proposals) {
  auto& self = state.local[node];
  if (self.status == kCompleted) {
    return state;
  }
  self.status = kCompleted;
  self.proposals = proposals;
  self.committed = proposals;
  state.commitMsgs = broadcastCommit(state.commitMsgs, state.alive, node, proposals);
  return state;
}

CalmState processVote(CalmState state,
    NodeId node,
    NodeId source,
    const ProposalSet& proposals,
    const NodeSet& incomingNodes) {
  const auto previous = state.local.at(node);
  if (previous.status == kCompleted || !previous.nodes.contains(source)) {
    return state;
  }

  auto status = previous.status;
  if (status == kMayCommit && previous.proposals != proposals) {
    status = kCannotCommit;
  }

  auto newProposals = setUnion(previous.proposals, proposals);
  auto newVoted = previous.voted;
  newVoted.insert(source);
  newVoted.insert(node);

  auto newNodes = previous.nodes;
  if (newNodes != incomingNodes) {
    if (status == kMayCommit) {
      status = kCannotCommit;
    }
    newNodes = setIntersection(newNodes, incomingNodes);
    newVoted = setIntersection(newVoted, incomingNodes);
  }

  state.local[node] =
      CalmNodeState{status, newNodes, newVoted, newProposals, previous.committed};

  if (newVoted == newNodes) {
    if (status == kMayCommit) {
      return commit(std::move(state), node, newProposals);
    }
    state.local[node].status = kToVote;
  }

  if (state.local.at(node).status == kToVote) {
    state.local[node].status = kMayCommit;
    const auto& current = state.local.at(node);
    state.voteMsgs = broadcastVote(
        state.voteMsgs, state.alive, node, current.proposals, current.nodes);
  }

  return state;
}

bool canPropose(const CalmState& state, NodeId node, MessageId id) {
  return state.alive.contains(node) &&
         !state.proposed.contains(id) &&
         state.local.at(node).voted.empty() &&
         state.local.at(node).status != kCompleted;
}

size_t quorumSize(const CalmState& state) {
  return state.local.size() / 2 + 1;
}

CalmState propose(CalmState state, NodeId node, MessageId id) {
  state.proposed.insert(id);
  auto nodes = state.local.at(node).nodes;
  return processVote(std::move(state), node, node, ProposalSet{id}, nodes);
}

bool canDeliverVote(const CalmState& state, const CalmVoteMsg& msg) {
  return state.alive.contains(msg.to);
}

CalmState deliverVote(CalmState state, const CalmVoteMsg& msg) {
  state.voteMsgs.erase(msg);
  return processVote(std::move(state), msg.to, msg.from, msg.proposals, msg.nodes);
}

bool canDeliverCommit(const CalmState& state, const CalmCommitMsg& msg) {
  return state.alive.contains(msg.to) &&
         state.local.at(msg.to).status != kCompleted &&
         state.local.at(msg.to).proposals == msg.commit;
}

CalmState deliverCommit(CalmState state, const CalmCommitMsg& msg) {
  state.commitMsgs.erase(msg);
  return commit(std::move(state), msg.to, msg.commit);
}

bool canDisconnect(const CalmState& state, NodeId failed) {
  return state.alive.contains(failed);
}

bool canLiveDisconnect(const CalmState& state, NodeId failed) {
  return canDisconnect(state, failed) && state.alive.size() - 1 >= quorumSize(state);
}

CalmState disconnect(CalmState state, NodeId failed) {
  if (!state.alive.contains(failed)) {
    return state;
  }

  state.alive.erase(failed);
  state.voteMsgs = purgeMessages(state.voteMsgs, failed);
  state.commitMsgs = purgeMessages(state.commitMsgs, failed);

  auto survivors = state.alive;
  for (auto&& node : survivors) {
    const auto current = state.local.at(node);
    if (current.proposals.empty()) {
      state.local[node].nodes.erase(failed);
      continue;
    }
    state = processVote(std::move(state), node, failed, current.proposals,
        setWithout(current.nodes, failed));
  }

  return state;
}

bool invariant(const CalmState& state) {
  if (!queueEndpointsAreAlive(state.voteMsgs, state.alive) ||
      !queueEndpointsAreAlive(state.commitMsgs, state.alive)) {
    return false;
  }

  for (auto&& node : state.alive) {
    const auto& self = state.local.at(node);
    if (!isSubset(self.proposals, state.proposed) || !isSubset(self.voted, self.nodes)) {
      return false;
    }
    if (self.status == kCompleted) {
      if (self.committed != self.proposals) {
        return false;
      }
    } else if (!self.committed.empty()) {
      return false;
    }
  }

  for (auto&& msg : state.voteMsgs) {
    if (!isSubset(msg.proposals, state.proposed)) {
      return false;
    }
  }

  for (auto&& left : state.alive) {
    const auto& committedLeft = state.local.at(left);
    if (committedLeft.status != kCompleted) {
      continue;
    }
    for (auto&& right : state.alive) {
      const auto& committedRight = state.local.at(right);
      if (committedRight.status == kCompleted &&
          committedLeft.committed != committedRight.committed) {
        return false;
      }
    }
  }

  return true;
}

bool commitHappened(const CalmState& state) {
  for (auto&& [node, local] : state.local) {
    if (local.status == kCompleted && !local.committed.empty()) {
      return true;
    }
  }
  return false;
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::calm::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::calm::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::calm::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::calm::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::calm::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::calm::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::calm::canDisconnect)
DEFINE_ALGORITHM(canLiveDisconnectExpr, ::leaderless_consensus::calm::canLiveDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::calm::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::calm::invariant)
DEFINE_ALGORITHM(commitHappenedExpr, ::leaderless_consensus::calm::commitHappened)

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
    return $E(msg, get_mem(state, commitMsgs)) {
      return canDeliverCommitExpr(state, msg) && state++ == deliverCommitExpr(state, msg);
    };
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(state);
  }

  Var<CalmState> state{"state"};

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

TEST_F(EngineFixture, CalmSafetyHoldsInvariant) {
  e.createModel<SafetyModel>();
  EXPECT_NO_THROW(e.run());
}

TEST_F(EngineFixture, CalmLivenessCommitsWithMajorityAlive) {
  e.createModel<LivenessModel>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::calm
