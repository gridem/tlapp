#include "common.h"

namespace leaderless_consensus::most {

constexpr int kVoting = 0;
constexpr int kCommitted = 1;

struct_fields(MostCarryVote, (int, id), (NodeSet, votes));

using CarryVotes = std::set<MostCarryVote>;

struct_fields(MostVoteMsg, (int, from), (int, to), (CarrySet, carries), (NodeSet, nodes));
struct_fields(MostCommitMsg, (int, from), (int, to), (CarrySet, commit));
struct_fields(MostNodeState,
    (int, status),
    (NodeSet, nodes),
    (NodeSet, votes),
    (CarryVotes, carryVotes),
    (CarrySet, carries),
    (CarrySet, committed));

using MostNodes = std::map<NodeId, MostNodeState>;
using MostVoteMessages = std::set<MostVoteMsg>;
using MostCommitMessages = std::set<MostCommitMsg>;

struct_fields(MostState,
    (NodeSet, alive),
    (CarrySet, applied),
    (MostNodes, local),
    (MostVoteMessages, voteMsgs),
    (MostCommitMessages, commitMsgs));

MostVoteMessages broadcastVote(const MostVoteMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const CarrySet& carries,
    const NodeSet& nodes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(MostVoteMsg{from, to, carries, nodes});
    }
  }
  return result;
}

MostCommitMessages broadcastCommit(const MostCommitMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const CarrySet& commit) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(MostCommitMsg{from, to, commit});
    }
  }
  return result;
}

MostState makeState(const NodeSet& nodes) {
  MostNodes local;
  for (auto&& node : nodes) {
    local[node] = MostNodeState{kVoting, nodes, {}, {}, {}, {}};
  }
  return MostState{nodes, {}, local, {}, {}};
}

MostState commit(MostState sys, NodeId node) {
  auto& self = sys.local[node];
  if (self.status == kCommitted) {
    return sys;
  }
  self.status = kCommitted;
  self.committed = self.carries;
  sys.commitMsgs = broadcastCommit(sys.commitMsgs, sys.alive, node, self.committed);
  return sys;
}

NodeSet carryVotesFor(const CarryVotes& carryVotes, MessageId id) {
  for (auto&& entry : carryVotes) {
    if (entry.id == id) {
      return entry.votes;
    }
  }
  return {};
}

CarryVotes putCarryVotes(CarryVotes carryVotes, MessageId id, const NodeSet& votes) {
  for (auto it = carryVotes.begin(); it != carryVotes.end(); ++it) {
    if (it->id == id) {
      carryVotes.erase(it);
      break;
    }
  }
  carryVotes.insert(MostCarryVote{id, votes});
  return carryVotes;
}

bool mayCommit(const MostNodeState& state) {
  for (auto&& entry : state.carryVotes) {
    auto id = entry.id;
    auto&& votes = entry.votes;
    if (!state.carries.contains(id)) {
      continue;
    }
    if (!(2 * votes.size() > state.nodes.size())) {
      return false;
    }
  }
  return true;
}

struct VoteResult {
  bool changed = false;
  bool sendVote = false;
  bool sendCommit = false;
  MostNodeState local;
};

VoteResult processVote(const MostNodeState& state,
    NodeId self,
    NodeId source,
    const CarrySet& carries,
    const NodeSet& incomingNodes) {
  if (state.status == kCommitted || !state.nodes.contains(source)) {
    return {false, false, false, state};
  }

  auto changedNodes = state.nodes != incomingNodes;
  auto firstVote = state.votes.empty();
  auto nodes = setIntersection(state.nodes, incomingNodes);
  auto newCarries = setUnion(state.carries, carries);
  auto votes = state.votes;
  votes.insert(self);
  votes.insert(source);
  votes = setIntersection(votes, nodes);

  auto carryVotes = state.carryVotes;
  for (auto&& id : carries) {
    auto votesFor = carryVotesFor(carryVotes, id);
    votesFor.insert(source);
    carryVotes = putCarryVotes(std::move(carryVotes), id, votesFor);
  }
  if (changedNodes) {
    CarryVotes filteredCarryVotes;
    for (auto&& entry : carryVotes) {
      filteredCarryVotes.insert(
          MostCarryVote{entry.id, setIntersection(entry.votes, nodes)});
    }
    carryVotes = std::move(filteredCarryVotes);
  }

  auto local =
      MostNodeState{kVoting, nodes, votes, carryVotes, newCarries, state.committed};
  if (firstVote) {
    return {local != state, true, false, local};
  }

  if (changedNodes) {
    local.votes = {self};
    if (local.votes != local.nodes) {
      return {local != state, true, false, local};
    }
  }

  if (local.votes != local.nodes) {
    return {local != state, false, false, local};
  }

  if (mayCommit(local)) {
    return {local != state, false, true, local};
  }
  return {local != state, true, false, local};
}

bool canPropose(const MostState& sys, NodeId node, MessageId id) {
  return sys.alive.contains(node) &&
         !sys.applied.contains(id) &&
         sys.local.at(node).votes.empty() &&
         sys.local.at(node).status != kCommitted;
}

MostState propose(MostState sys, NodeId node, MessageId id) {
  sys.applied.insert(id);
  auto nodes = sys.local.at(node).nodes;
  auto out = processVote(sys.local.at(node), node, node, CarrySet{id}, nodes);
  if (!out.changed) {
    return sys;
  }
  sys.local[node] = out.local;
  if (out.sendCommit) {
    return commit(std::move(sys), node);
  }
  if (out.sendVote) {
    sys.voteMsgs =
        broadcastVote(sys.voteMsgs, sys.alive, node, out.local.carries, out.local.nodes);
  }
  return sys;
}

bool canDeliverVote(const MostState& sys, const MostVoteMsg& msg) {
  return sys.alive.contains(msg.to);
}

MostState deliverVote(MostState sys, const MostVoteMsg& msg) {
  sys.voteMsgs.erase(msg);
  auto out = processVote(sys.local.at(msg.to), msg.to, msg.from, msg.carries, msg.nodes);
  if (!out.changed) {
    return sys;
  }
  sys.local[msg.to] = out.local;
  if (out.sendCommit) {
    return commit(std::move(sys), msg.to);
  }
  if (out.sendVote) {
    sys.voteMsgs = broadcastVote(
        sys.voteMsgs, sys.alive, msg.to, out.local.carries, out.local.nodes);
  }
  return sys;
}

bool canDeliverCommit(const MostState& sys, const MostCommitMsg& msg) {
  return sys.alive.contains(msg.to) &&
         sys.local.at(msg.to).status != kCommitted &&
         sys.local.at(msg.to).carries == msg.commit;
}

MostState deliverCommit(MostState sys, const MostCommitMsg& msg) {
  sys.commitMsgs.erase(msg);
  auto& self = sys.local[msg.to];
  self.status = kCommitted;
  self.carries = msg.commit;
  self.committed = msg.commit;
  sys.commitMsgs = broadcastCommit(sys.commitMsgs, sys.alive, msg.to, msg.commit);
  return sys;
}

bool canDisconnect(const MostState& sys, NodeId failed) {
  return sys.alive.contains(failed);
}

MostState disconnect(MostState sys, NodeId failed) {
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

    auto out = processVote(
        current, node, failed, current.carries, setWithout(current.nodes, failed));
    if (!out.changed) {
      continue;
    }
    sys.local[node] = out.local;
    if (out.sendCommit) {
      sys = commit(std::move(sys), node);
    } else if (out.sendVote) {
      sys.voteMsgs = broadcastVote(
          sys.voteMsgs, sys.alive, node, out.local.carries, out.local.nodes);
    }
  }

  return sys;
}

bool invariant(const MostState& sys) {
  if (!queueEndpointsAreAlive(sys.voteMsgs, sys.alive) ||
      !queueEndpointsAreAlive(sys.commitMsgs, sys.alive)) {
    return false;
  }

  NodeSet allNodes;
  for (auto&& [node, _] : sys.local) {
    allNodes.insert(node);
  }

  for (auto&& node : sys.alive) {
    const auto& self = sys.local.at(node);
    if (!isSubset(self.carries, sys.applied) || !isSubset(self.votes, self.nodes)) {
      return false;
    }
    for (auto&& entry : self.carryVotes) {
      if (!self.carries.contains(entry.id) || !isSubset(entry.votes, allNodes)) {
        return false;
      }
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

bool quiescent(const MostState& sys) {
  if (!sys.voteMsgs.empty() || !sys.commitMsgs.empty()) {
    return false;
  }

  for (auto&& node : sys.alive) {
    if (sys.local.at(node).votes.empty() &&
        sys.local.at(node).status != kCommitted &&
        sys.applied.size() < 3) {
      return false;
    }
  }

  return true;
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::most::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::most::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::most::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::most::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::most::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::most::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::most::canDisconnect)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::most::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::most::invariant)
DEFINE_ALGORITHM(quiescentExpr, ::leaderless_consensus::most::quiescent)

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

  Var<MostState> sys{"sys"};

  NodeSet nodes_ = {0, 1, 2};
  CarrySet messageIds_ = {10, 11, 12};
};

TEST_F(EngineFixture, MostHoldsInvariantAndConverges) {
  e.createModel<Model>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::most
