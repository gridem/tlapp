#include "common.h"

namespace leaderless_consensus::sore {

constexpr int kInitial = 0;
constexpr int kVoted = 1;
constexpr int kCompleted = 2;

struct_fields(SoreVoteMsg,
    (int, from),
    (int, to),
    (ProposalSet, proposals),
    (NodeSet, nodes));
struct_fields(SoreCommitMsg, (int, from), (int, to), (ProposalSet, commit));
struct_fields(SoreNodeState,
    (int, status),
    (NodeSet, nodes),
    (NodeSet, voted),
    (ProposalSet, proposals),
    (ProposalSet, committed));

using SoreNodes = FlatMap<NodeId, SoreNodeState>;
using SoreVoteMessages = FlatSet<SoreVoteMsg>;
using SoreCommitMessages = FlatSet<SoreCommitMsg>;

struct_fields(SoreState,
    (NodeSet, alive),
    (ProposalSet, proposed),
    (SoreNodes, local),
    (SoreVoteMessages, voteMsgs),
    (SoreCommitMessages, commitMsgs));

SoreVoteMessages broadcastVote(const SoreVoteMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& proposals,
    const NodeSet& nodes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(SoreVoteMsg{from, to, proposals, nodes});
    }
  }
  return result;
}

SoreCommitMessages broadcastCommit(const SoreCommitMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& commit) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(SoreCommitMsg{from, to, commit});
    }
  }
  return result;
}

SoreState makeState(const NodeSet& nodes) {
  SoreNodes local;
  for (auto&& node : nodes) {
    local[node] = SoreNodeState{kInitial, nodes, {}, {}, {}};
  }
  return SoreState{nodes, {}, local, {}, {}};
}

SoreState commit(SoreState state, NodeId node, const ProposalSet& proposals) {
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

struct VoteResult {
  bool changed = false;
  bool sendVote = false;
  bool sendCommit = false;
  ProposalSet commitSet;
  SoreNodeState local;
};

VoteResult processVote(const SoreNodeState& state,
    NodeId self,
    NodeId source,
    const ProposalSet& proposals,
    const NodeSet& incomingNodes) {
  if (state.status == kCompleted || !state.nodes.contains(source)) {
    return {false, false, false, {}, state};
  }

  auto status = state.status;
  auto nodes = state.nodes;
  auto voted = state.voted;
  auto newProposals = setUnion(state.proposals, proposals);

  if (nodes != incomingNodes) {
    status = kInitial;
    nodes = setIntersection(nodes, incomingNodes);
    voted.clear();
  }

  voted.insert(source);
  voted.insert(self);
  voted = setIntersection(voted, nodes);

  auto local = SoreNodeState{status, nodes, voted, newProposals, state.committed};
  if (voted == nodes) {
    return {local != state, false, true, newProposals, local};
  }
  if (status == kInitial) {
    return {true, true, false, {},
        SoreNodeState{kVoted, nodes, voted, newProposals, state.committed}};
  }
  return {local != state, false, false, {}, local};
}

bool canPropose(const SoreState& state, NodeId node, MessageId id) {
  return state.alive.contains(node) &&
         !state.proposed.contains(id) &&
         state.local.at(node).voted.empty() &&
         state.local.at(node).status != kCompleted;
}

SoreState propose(SoreState state, NodeId node, MessageId id) {
  state.proposed.insert(id);
  auto nodes = state.local.at(node).nodes;
  auto out = processVote(state.local.at(node), node, node, ProposalSet{id}, nodes);
  if (!out.changed) {
    return state;
  }
  state.local[node] = out.local;
  if (out.sendCommit) {
    return commit(std::move(state), node, out.commitSet);
  }
  if (out.sendVote) {
    state.voteMsgs = broadcastVote(
        state.voteMsgs, state.alive, node, out.local.proposals, out.local.nodes);
  }
  return state;
}

bool canDeliverVote(const SoreState& state, const SoreVoteMsg& msg) {
  return state.alive.contains(msg.to);
}

SoreState deliverVote(SoreState state, const SoreVoteMsg& msg) {
  state.voteMsgs.erase(msg);
  auto out =
      processVote(state.local.at(msg.to), msg.to, msg.from, msg.proposals, msg.nodes);
  if (!out.changed) {
    return state;
  }
  state.local[msg.to] = out.local;
  if (out.sendCommit) {
    return commit(std::move(state), msg.to, out.commitSet);
  }
  if (out.sendVote) {
    state.voteMsgs = broadcastVote(
        state.voteMsgs, state.alive, msg.to, out.local.proposals, out.local.nodes);
  }
  return state;
}

bool canDeliverCommit(const SoreState& state, const SoreCommitMsg& msg) {
  return state.alive.contains(msg.to) && state.local.at(msg.to).status != kCompleted;
}

SoreState deliverCommit(SoreState state, const SoreCommitMsg& msg) {
  state.commitMsgs.erase(msg);
  return commit(std::move(state), msg.to, msg.commit);
}

bool canDisconnect(const SoreState& state, NodeId failed) {
  return state.alive.contains(failed);
}

SoreState disconnect(SoreState state, NodeId failed) {
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

    auto out = processVote(
        current, node, failed, current.proposals, setWithout(current.nodes, failed));
    if (!out.changed) {
      continue;
    }
    state.local[node] = out.local;
    if (out.sendCommit) {
      state = commit(std::move(state), node, out.commitSet);
      continue;
    }
    if (out.sendVote) {
      state.voteMsgs = broadcastVote(
          state.voteMsgs, state.alive, node, out.local.proposals, out.local.nodes);
    }
  }

  return state;
}

bool invariant(const SoreState& state) {
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

  for (auto&& msg : state.commitMsgs) {
    if (!isSubset(msg.commit, state.proposed)) {
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

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::sore::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::sore::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::sore::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::sore::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::sore::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::sore::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::sore::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::sore::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::sore::invariant)

struct Model : IModel {
  Boolean init() override {
    return state == makeState(nodes_);
  }

  Boolean next() override {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canProposeExpr(state, node, id) && state++ == proposeExpr(state, node, id);
      };
    }
    || $E(msg, get_mem(state, voteMsgs)) {
      return canDeliverVoteExpr(state, msg) && state++ == deliverVoteExpr(state, msg);
    }
    || $E(msg, get_mem(state, commitMsgs)) {
      return canDeliverCommitExpr(state, msg) && state++ == deliverCommitExpr(state, msg);
    }
    || $E(failed, nodes_) {
      return canDisconnectExpr(state, failed) && state++ == disconnectExpr(state, failed);
    };
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(state);
  }

  Var<SoreState> state{"state"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = kProposalIds;
};

TEST_F(EngineFixture, SoreFindsCounterexample) {
  e.createModel<Model>();
  // This variant is intentionally kept as a negative baseline.
  EXPECT_THROW(e.run(), EnsureError);
}

}  // namespace leaderless_consensus::sore
