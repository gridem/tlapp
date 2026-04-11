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

CalmState commit(CalmState sys, NodeId node, const ProposalSet& proposals) {
  auto& self = sys.local[node];
  if (self.status == kCompleted) {
    return sys;
  }
  self.status = kCompleted;
  self.proposals = proposals;
  self.committed = proposals;
  sys.commitMsgs = broadcastCommit(sys.commitMsgs, sys.alive, node, proposals);
  return sys;
}

CalmState processVote(CalmState sys,
    NodeId node,
    NodeId source,
    const ProposalSet& proposals,
    const NodeSet& incomingNodes) {
  const auto previous = sys.local.at(node);
  if (previous.status == kCompleted || !previous.nodes.contains(source)) {
    return sys;
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

  sys.local[node] =
      CalmNodeState{status, newNodes, newVoted, newProposals, previous.committed};

  if (newVoted == newNodes) {
    if (status == kMayCommit) {
      return commit(std::move(sys), node, newProposals);
    }
    sys.local[node].status = kToVote;
  }

  if (sys.local.at(node).status == kToVote) {
    sys.local[node].status = kMayCommit;
    const auto& current = sys.local.at(node);
    sys.voteMsgs =
        broadcastVote(sys.voteMsgs, sys.alive, node, current.proposals, current.nodes);
  }

  return sys;
}

bool canPropose(const CalmState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) &&
         !sys.proposed.contains(id) &&
         sys.local.at(node).voted.empty() &&
         sys.local.at(node).status != kCompleted;
}

CalmState propose(CalmState sys, NodeId node, MessageId id) {
  sys.proposed.insert(id);
  auto nodes = sys.local.at(node).nodes;
  return processVote(std::move(sys), node, node, ProposalSet{id}, nodes);
}

bool canDeliverVote(const CalmState& sys, const CalmVoteMsg& msg) {
  return sys.alive.contains(msg.to);
}

CalmState deliverVote(CalmState sys, const CalmVoteMsg& msg) {
  sys.voteMsgs.erase(msg);
  return processVote(std::move(sys), msg.to, msg.from, msg.proposals, msg.nodes);
}

bool canDeliverCommit(const CalmState& sys, const CalmCommitMsg& msg) {
  return sys.alive.contains(msg.to) &&
         sys.local.at(msg.to).status != kCompleted &&
         sys.local.at(msg.to).proposals == msg.commit;
}

CalmState deliverCommit(CalmState sys, const CalmCommitMsg& msg) {
  sys.commitMsgs.erase(msg);
  return commit(std::move(sys), msg.to, msg.commit);
}

bool canDisconnect(const CalmState& sys, NodeId failed) {
  return sys.alive.contains(failed);
}

CalmState disconnect(CalmState sys, NodeId failed) {
  if (!sys.alive.contains(failed)) {
    return sys;
  }

  sys.alive.erase(failed);
  sys.voteMsgs = purgeMessages(sys.voteMsgs, failed);
  sys.commitMsgs = purgeMessages(sys.commitMsgs, failed);

  auto survivors = sys.alive;
  for (auto&& node : survivors) {
    const auto current = sys.local.at(node);
    if (current.proposals.empty()) {
      sys.local[node].nodes.erase(failed);
      continue;
    }
    sys = processVote(std::move(sys), node, failed, current.proposals,
        setWithout(current.nodes, failed));
  }

  return sys;
}

bool invariant(const CalmState& sys) {
  if (!queueEndpointsAreAlive(sys.voteMsgs, sys.alive) ||
      !queueEndpointsAreAlive(sys.commitMsgs, sys.alive)) {
    return false;
  }

  for (auto&& node : sys.alive) {
    const auto& self = sys.local.at(node);
    if (!isSubset(self.proposals, sys.proposed) || !isSubset(self.voted, self.nodes)) {
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

  for (auto&& msg : sys.voteMsgs) {
    if (!isSubset(msg.proposals, sys.proposed)) {
      return false;
    }
  }

  for (auto&& left : sys.alive) {
    const auto& committedLeft = sys.local.at(left);
    if (committedLeft.status != kCompleted) {
      continue;
    }
    for (auto&& right : sys.alive) {
      const auto& committedRight = sys.local.at(right);
      if (committedRight.status == kCompleted &&
          committedLeft.committed != committedRight.committed) {
        return false;
      }
    }
  }

  return true;
}

bool quiescent(const CalmState& sys) {
  if (!sys.voteMsgs.empty() || !sys.commitMsgs.empty()) {
    return false;
  }

  for (auto&& node : sys.alive) {
    if (sys.local.at(node).voted.empty() &&
        sys.local.at(node).status != kCompleted &&
        sys.proposed.size() < 3) {
      return false;
    }
  }

  return true;
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::calm::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::calm::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::calm::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::calm::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::calm::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::calm::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::calm::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::calm::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::calm::invariant)
DEFINE_ALGORITHM(quiescentExpr, ::leaderless_consensus::calm::quiescent)

struct Model : IModel {
  Boolean init() override {
    return sys == makeState(nodes_);
  }

  Boolean next() override {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canProposeExpr(sys, node, id) && sys++ == proposeExpr(sys, node, id);
      };
    }
    || $E(msg, get_mem(sys, voteMsgs)) {
      return canDeliverVoteExpr(sys, msg) && sys++ == deliverVoteExpr(sys, msg);
    }
    || $E(msg, get_mem(sys, commitMsgs)) {
      return canDeliverCommitExpr(sys, msg) && sys++ == deliverCommitExpr(sys, msg);
    }
    || $E(failed, nodes_) {
      return canDisconnectExpr(sys, failed) && sys++ == disconnectExpr(sys, failed);
    };
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(sys);
  }

  std::optional<LivenessBoolean> liveness() override {
    return wf(next()) && eventually(quiescentExpr(sys));
  }

  Var<CalmState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = {10, 11, 12};
};

TEST_F(EngineFixture, CalmHoldsInvariantAndConverges) {
  e.createModel<Model>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::calm
