#include "common.h"

namespace leaderless_consensus::flat {

constexpr int kVoting = 0;
constexpr int kCommitted = 1;

struct_fields(FlatVoteMsg, (int, from), (int, to), (CarrySet, carries),
              (NodeSet, nodes), (NodeSet, votes));
struct_fields(FlatCommitMsg, (int, from), (int, to));
struct_fields(FlatNodeState, (int, status), (NodeSet, nodes), (NodeSet, votes),
              (CarrySet, carries), (CarrySet, committed));

using FlatNodes = std::map<NodeId, FlatNodeState>;
using FlatVoteMessages = std::set<FlatVoteMsg>;
using FlatCommitMessages = std::set<FlatCommitMsg>;

struct_fields(FlatState, (NodeSet, alive), (CarrySet, applied),
              (FlatNodes, local), (FlatVoteMessages, voteMsgs),
              (FlatCommitMessages, commitMsgs));

FlatVoteMessages broadcastVote(const FlatVoteMessages& messages,
                               const NodeSet& alive, NodeId from,
                               const CarrySet& carries, const NodeSet& nodes,
                               const NodeSet& votes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(FlatVoteMsg{from, to, carries, nodes, votes});
    }
  }
  return result;
}

FlatCommitMessages broadcastCommit(const FlatCommitMessages& messages,
                                   const NodeSet& alive, NodeId from) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(FlatCommitMsg{from, to});
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
  self.committed = self.carries;
  sys.commitMsgs = broadcastCommit(sys.commitMsgs, sys.alive, node);
  return sys;
}

FlatState processVote(FlatState sys, NodeId node, NodeId source,
                      const CarrySet& carries,
                      const NodeSet& incomingNodes,
                      const NodeSet& incomingVotes) {
  const auto previous = sys.local.at(node);
  if (previous.status == kCommitted || !previous.nodes.contains(source)) {
    return sys;
  }

  auto newNodes = setIntersection(incomingNodes, previous.nodes);
  auto newCarries = setUnion(previous.carries, carries);
  NodeSet newVotes = {node};

  if (newNodes == incomingNodes && newCarries == carries) {
    newVotes = setUnion(newVotes, incomingVotes);
  }
  if (newNodes == previous.nodes && newCarries == previous.carries) {
    newVotes = setUnion(newVotes, previous.votes);
  }
  newVotes = setIntersection(newVotes, newNodes);

  if (newNodes == previous.nodes && newCarries == previous.carries &&
      newVotes == previous.votes) {
    return sys;
  }

  sys.local[node] =
      FlatNodeState{kVoting, newNodes, newVotes, newCarries,
                    previous.committed};

  if (newNodes == newVotes) {
    return commit(std::move(sys), node);
  }

  sys.voteMsgs = broadcastVote(sys.voteMsgs, sys.alive, node, newCarries,
                               newNodes, newVotes);
  return sys;
}

bool canApply(const FlatState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) && !sys.applied.contains(id) &&
         id == node + 1 &&
         sys.local.at(node).votes.empty() &&
         sys.local.at(node).status != kCommitted;
}

FlatState apply(FlatState sys, NodeId node, MessageId id) {
  sys.applied.insert(id);
  auto nodes = sys.local.at(node).nodes;
  return processVote(std::move(sys), node, node, CarrySet{id}, nodes, {});
}

bool canDeliverVote(const FlatState& sys, const FlatVoteMsg& msg) {
  return sys.alive.contains(msg.to);
}

FlatState deliverVote(FlatState sys, const FlatVoteMsg& msg) {
  sys.voteMsgs.erase(msg);
  return processVote(std::move(sys), msg.to, msg.from, msg.carries, msg.nodes,
                     msg.votes);
}

bool canDeliverCommit(const FlatState& sys, const FlatCommitMsg& msg) {
  return sys.alive.contains(msg.to) &&
         sys.local.at(msg.to).status != kCommitted;
}

FlatState deliverCommit(FlatState sys, const FlatCommitMsg& msg) {
  sys.commitMsgs.erase(msg);
  return commit(std::move(sys), msg.to);
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
  sys.commitMsgs = purgeMessages(sys.commitMsgs, failed);

  auto survivors = sys.alive;
  for (auto&& node : survivors) {
    const auto current = sys.local.at(node);
    if (current.carries.empty()) {
      sys.local[node].nodes.erase(failed);
      continue;
    }
    sys = processVote(std::move(sys), node, failed, current.carries,
                      setWithout(current.nodes, failed), {});
  }

  return sys;
}

bool invariant(const FlatState& sys) {
  if (!queueEndpointsAreAlive(sys.voteMsgs, sys.alive) ||
      !queueEndpointsAreAlive(sys.commitMsgs, sys.alive)) {
    return false;
  }

  for (auto&& node : sys.alive) {
    const auto& self = sys.local.at(node);
    if (!isSubset(self.carries, sys.applied) ||
        !isSubset(self.votes, self.nodes)) {
      return false;
    }
    if (self.status == kCommitted) {
      if (self.committed != self.carries) {
        return false;
      }
    } else if (!self.committed.empty()) {
      return false;
    }
  }

  for (auto&& msg : sys.voteMsgs) {
    if (!isSubset(msg.carries, sys.applied) ||
        !isSubset(msg.votes, msg.nodes)) {
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

DEFINE_ALGORITHM(canApplyExpr, ::leaderless_consensus::flat::canApply)
DEFINE_ALGORITHM(applyExpr, ::leaderless_consensus::flat::apply)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::flat::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::flat::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr,
                 ::leaderless_consensus::flat::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::flat::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::flat::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::flat::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::flat::invariant)

struct Model : IModel {
  Boolean init() override { return sys == makeState(nodes_); }

  Boolean next() override {
    return $E(node, nodes_) {
      return $E(id, messageIds_) {
        return canApplyExpr(sys, node, id) &&
               sys++ == applyExpr(sys, node, id);
      };
    }
    || $E(msg, get_mem(sys, voteMsgs)) {
      return canDeliverVoteExpr(sys, msg) &&
             sys++ == deliverVoteExpr(sys, msg);
    }
    || $E(msg, get_mem(sys, commitMsgs)) {
      return canDeliverCommitExpr(sys, msg) &&
             sys++ == deliverCommitExpr(sys, msg);
    }
    || $E(failed, nodes_) {
      return canDisconnectExpr(sys, failed) &&
             sys++ == disconnectExpr(sys, failed);
    };
  }

  std::optional<Boolean> ensure() override { return invariantExpr(sys); }

  Var<FlatState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  CarrySet messageIds_ = {1, 2, 3};
};

TEST_F(EngineFixture, DISABLED_FlatExploration) {
  e.createModel<Model>();
  e.run();
}

}  // namespace leaderless_consensus::flat
