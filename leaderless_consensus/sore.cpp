#include "common.h"

namespace leaderless_consensus::sore {

constexpr int kInitial = 0;
constexpr int kVoted = 1;
constexpr int kCompleted = 2;

struct_fields(SoreVoteMsg, (int, from), (int, to), (CarrySet, carries),
              (NodeSet, nodes));
struct_fields(SoreCommitMsg, (int, from), (int, to), (CarrySet, commit));
struct_fields(SoreNodeState, (int, status), (NodeSet, nodes), (NodeSet, voted),
              (CarrySet, carries), (CarrySet, committed));

using SoreNodes = std::map<NodeId, SoreNodeState>;
using SoreVoteMessages = std::set<SoreVoteMsg>;
using SoreCommitMessages = std::set<SoreCommitMsg>;

struct_fields(SoreState, (NodeSet, alive), (CarrySet, applied),
              (SoreNodes, local), (SoreVoteMessages, voteMsgs),
              (SoreCommitMessages, commitMsgs));

SoreVoteMessages broadcastVote(const SoreVoteMessages& messages,
                               const NodeSet& alive, NodeId from,
                               const CarrySet& carries, const NodeSet& nodes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(SoreVoteMsg{from, to, carries, nodes});
    }
  }
  return result;
}

SoreCommitMessages broadcastCommit(const SoreCommitMessages& messages,
                                   const NodeSet& alive, NodeId from,
                                   const CarrySet& commit) {
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

SoreState commit(SoreState sys, NodeId node, const CarrySet& carries) {
  auto& self = sys.local[node];
  if (self.status == kCompleted) {
    return sys;
  }
  self.status = kCompleted;
  self.carries = carries;
  self.committed = carries;
  sys.commitMsgs = broadcastCommit(sys.commitMsgs, sys.alive, node, carries);
  return sys;
}

struct VoteResult {
  bool changed = false;
  bool sendVote = false;
  bool sendCommit = false;
  CarrySet commitSet;
  SoreNodeState local;
};

VoteResult processVote(const SoreNodeState& state, NodeId self, NodeId source,
                       const CarrySet& carries,
                       const NodeSet& incomingNodes) {
  if (state.status == kCompleted || !state.nodes.contains(source)) {
    return {false, false, false, {}, state};
  }

  auto status = state.status;
  auto nodes = state.nodes;
  auto voted = state.voted;
  auto newCarries = setUnion(state.carries, carries);

  if (nodes != incomingNodes) {
    status = kInitial;
    nodes = setIntersection(nodes, incomingNodes);
    voted.clear();
  }

  voted.insert(source);
  voted.insert(self);
  voted = setIntersection(voted, nodes);

  auto local = SoreNodeState{status, nodes, voted, newCarries, state.committed};
  if (voted == nodes) {
    return {local != state, false, true, newCarries, local};
  }
  if (status == kInitial) {
    return {true, true, false, {},
            SoreNodeState{kVoted, nodes, voted, newCarries, state.committed}};
  }
  return {local != state, false, false, {}, local};
}

bool canApply(const SoreState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) && !sys.applied.contains(id) &&
         sys.local.at(node).voted.empty() &&
         sys.local.at(node).status != kCompleted;
}

SoreState apply(SoreState sys, NodeId node, MessageId id) {
  sys.applied.insert(id);
  auto nodes = sys.local.at(node).nodes;
  auto out = processVote(sys.local.at(node), node, node, CarrySet{id}, nodes);
  if (!out.changed) {
    return sys;
  }
  sys.local[node] = out.local;
  if (out.sendCommit) {
    return commit(std::move(sys), node, out.commitSet);
  }
  if (out.sendVote) {
    sys.voteMsgs = broadcastVote(sys.voteMsgs, sys.alive, node,
                                 out.local.carries, out.local.nodes);
  }
  return sys;
}

bool canDeliverVote(const SoreState& sys, const SoreVoteMsg& msg) {
  return sys.alive.contains(msg.to);
}

SoreState deliverVote(SoreState sys, const SoreVoteMsg& msg) {
  sys.voteMsgs.erase(msg);
  auto out = processVote(sys.local.at(msg.to), msg.to, msg.from, msg.carries,
                         msg.nodes);
  if (!out.changed) {
    return sys;
  }
  sys.local[msg.to] = out.local;
  if (out.sendCommit) {
    return commit(std::move(sys), msg.to, out.commitSet);
  }
  if (out.sendVote) {
    sys.voteMsgs = broadcastVote(sys.voteMsgs, sys.alive, msg.to,
                                 out.local.carries, out.local.nodes);
  }
  return sys;
}

bool canDeliverCommit(const SoreState& sys, const SoreCommitMsg& msg) {
  return sys.alive.contains(msg.to) &&
         sys.local.at(msg.to).status != kCompleted;
}

SoreState deliverCommit(SoreState sys, const SoreCommitMsg& msg) {
  sys.commitMsgs.erase(msg);
  return commit(std::move(sys), msg.to, msg.commit);
}

bool canDisconnect(const SoreState& sys, NodeId failed) {
  return sys.alive.contains(failed);
}

SoreState disconnect(SoreState sys, NodeId failed) {
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

    auto out =
        processVote(current, node, failed, current.carries,
                    setWithout(current.nodes, failed));
    if (!out.changed) {
      continue;
    }
    sys.local[node] = out.local;
    if (out.sendCommit) {
      sys = commit(std::move(sys), node, out.commitSet);
      continue;
    }
    if (out.sendVote) {
      sys.voteMsgs = broadcastVote(sys.voteMsgs, sys.alive, node,
                                   out.local.carries, out.local.nodes);
    }
  }

  return sys;
}

bool invariant(const SoreState& sys) {
  if (!queueEndpointsAreAlive(sys.voteMsgs, sys.alive) ||
      !queueEndpointsAreAlive(sys.commitMsgs, sys.alive)) {
    return false;
  }

  for (auto&& node : sys.alive) {
    const auto& self = sys.local.at(node);
    if (!isSubset(self.carries, sys.applied) ||
        !isSubset(self.voted, self.nodes)) {
      return false;
    }
    if (self.status == kCompleted) {
      if (self.committed != self.carries) {
        return false;
      }
    } else if (!self.committed.empty()) {
      return false;
    }
  }

  for (auto&& msg : sys.voteMsgs) {
    if (!isSubset(msg.carries, sys.applied)) {
      return false;
    }
  }

  for (auto&& msg : sys.commitMsgs) {
    if (!isSubset(msg.commit, sys.applied)) {
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

DEFINE_ALGORITHM(canApplyExpr, ::leaderless_consensus::sore::canApply)
DEFINE_ALGORITHM(applyExpr, ::leaderless_consensus::sore::apply)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::sore::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::sore::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr,
                 ::leaderless_consensus::sore::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::sore::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::sore::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::sore::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::sore::invariant)

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

  Var<SoreState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  CarrySet messageIds_ = {10, 11, 12};
};

TEST_F(EngineFixture, SoreFindsCounterexample) {
  e.createModel<Model>();
  // This variant is intentionally kept as a negative baseline.
  EXPECT_THROW(e.run(), EnsureError);
}

}  // namespace leaderless_consensus::sore
