#include "common.h"

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

using FlatNodes = std::map<NodeId, FlatNodeState>;
using FlatVoteMessages = std::set<FlatVoteMsg>;
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

FlatCommitMessages broadcastCommit(const FlatState& sys, NodeId from) {
  auto result = setWithout(sys.commitMsgs, from);
  for (auto&& to : sys.alive) {
    if (to != from && sys.local.at(to).status != kCommitted) {
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

FlatState commit(FlatState sys, NodeId node) {
  auto& self = sys.local[node];
  if (self.status == kCommitted) {
    return sys;
  }
  self.status = kCommitted;
  self.committed = self.proposals;
  sys.voteMsgs = purgeVotesTo(sys.voteMsgs, node);
  sys.commitMsgs = setWithout(sys.commitMsgs, node);
  sys.commitMsgs = broadcastCommit(sys, node);
  return sys;
}

FlatState processVote(FlatState sys,
    NodeId node,
    NodeId source,
    const ProposalSet& proposals,
    const NodeSet& incomingNodes,
    const NodeSet& incomingVotes) {
  const auto previous = sys.local.at(node);
  if (previous.status == kCommitted || !previous.nodes.contains(source)) {
    return sys;
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
    return sys;
  }

  sys.local[node] =
      FlatNodeState{kVoting, newNodes, newVotes, newProposals, previous.committed};

  if (newNodes == newVotes) {
    return commit(std::move(sys), node);
  }

  sys.voteMsgs =
      broadcastVote(sys.voteMsgs, sys.alive, node, newProposals, newNodes, newVotes);
  return sys;
}

bool canPropose(const FlatState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) &&
         !sys.proposed.contains(id) &&
         id == node + 10 &&
         sys.local.at(node).votes.empty() &&
         sys.local.at(node).status != kCommitted;
}

FlatState propose(FlatState sys, NodeId node, MessageId id) {
  sys.proposed.insert(id);
  auto nodes = sys.local.at(node).nodes;
  return processVote(std::move(sys), node, node, ProposalSet{id}, nodes, {});
}

bool canDeliverVote(const FlatState& sys, const FlatVoteMsg& msg) {
  return sys.alive.contains(msg.to);
}

FlatState deliverVote(FlatState sys, const FlatVoteMsg& msg) {
  sys.voteMsgs.erase(msg);
  return processVote(
      std::move(sys), msg.to, msg.from, msg.proposals, msg.nodes, msg.votes);
}

bool canDeliverCommit(const FlatState& sys, NodeId node) {
  return sys.alive.contains(node) && sys.local.at(node).status != kCommitted;
}

FlatState deliverCommit(FlatState sys, NodeId node) {
  sys.commitMsgs.erase(node);
  return commit(std::move(sys), node);
}

bool canDisconnect(const FlatState& sys, NodeId failed) {
  return sys.alive.contains(failed);
}

FlatState disconnect(FlatState sys, NodeId failed) {
  if (!sys.alive.contains(failed)) {
    return sys;
  }

  sys.alive.erase(failed);
  sys.voteMsgs = purgeMessages(sys.voteMsgs, failed);
  sys.commitMsgs.erase(failed);

  auto survivors = sys.alive;
  for (auto&& node : survivors) {
    const auto current = sys.local.at(node);
    if (current.proposals.empty()) {
      sys.local[node].nodes.erase(failed);
      continue;
    }
    sys = processVote(std::move(sys), node, failed, current.proposals,
        setWithout(current.nodes, failed), {});
  }

  return sys;
}

bool invariant(const FlatState& sys) {
  if (!queueEndpointsAreAlive(sys.voteMsgs, sys.alive) ||
      !isSubset(sys.commitMsgs, sys.alive)) {
    return false;
  }

  for (auto&& node : sys.alive) {
    const auto& self = sys.local.at(node);
    if (!isSubset(self.proposals, sys.proposed) || !isSubset(self.votes, self.nodes)) {
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

  for (auto&& msg : sys.voteMsgs) {
    if (!isSubset(msg.proposals, sys.proposed) || !isSubset(msg.votes, msg.nodes)) {
      return false;
    }
  }

  for (auto&& left : sys.alive) {
    const auto& committedLeft = sys.local.at(left);
    if (committedLeft.status != kCommitted) {
      continue;
    }
    for (auto&& right : sys.alive) {
      const auto& committedRight = sys.local.at(right);
      if (committedRight.status == kCommitted &&
          committedLeft.committed != committedRight.committed) {
        return false;
      }
    }
  }

  return true;
}

bool canProposeAny(const FlatState& sys) {
  if (sys.proposed.size() >= sys.local.size()) {
    return false;
  }

  for (auto&& node : sys.alive) {
    if (sys.local.at(node).votes.empty() && sys.local.at(node).status != kCommitted) {
      return true;
    }
  }

  return false;
}

bool quiescent(const FlatState& sys) {
  return sys.voteMsgs.empty() && sys.commitMsgs.empty() && !canProposeAny(sys);
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::flat::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::flat::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::flat::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::flat::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::flat::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::flat::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::flat::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::flat::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::flat::invariant)
DEFINE_ALGORITHM(quiescentExpr, ::leaderless_consensus::flat::quiescent)

struct Model : IModel {
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

  Boolean deliverAnyVote() {
    return $E(msg, get_mem(sys, voteMsgs)) {
      return canDeliverVoteExpr(sys, msg) && sys++ == deliverVoteExpr(sys, msg);
    };
  }

  Boolean deliverAnyCommit() {
    return $E(node, get_mem(sys, commitMsgs)) {
      return canDeliverCommitExpr(sys, node) && sys++ == deliverCommitExpr(sys, node);
    };
  }

  Boolean disconnectAny() {
    return $E(failed, nodes_) {
      return canDisconnectExpr(sys, failed) && sys++ == disconnectExpr(sys, failed);
    };
  }

  Boolean next() override {
    return proposeAny() || deliverAnyVote() || deliverAnyCommit() || disconnectAny();
  }

  std::optional<Boolean> ensure() override {
    return invariantExpr(sys);
  }

  std::optional<LivenessBoolean> liveness() override {
    return wf(proposeAny()) && wf(deliverAnyVote()) && eventually(quiescentExpr(sys));
  }

  Var<FlatState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  ProposalSet messageIds_ = {10, 11, 12};
};

TEST_F(EngineFixture, FlatHoldsInvariantAndConverges) {
  e.createModel<Model>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::flat
