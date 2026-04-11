#include "model_common.h"

namespace leaderless_consensus::most {

constexpr int kVoting = 0;
constexpr int kCommitted = 1;

struct_fields(MostProposalVote, (int, id), (NodeSet, votes));

using ProposalVotes = FlatSet<MostProposalVote>;

struct_fields(MostVoteMsg,
    (int, from),
    (int, to),
    (ProposalSet, proposals),
    (NodeSet, nodes));
struct_fields(MostCommitMsg, (int, from), (int, to), (ProposalSet, commit));
struct_fields(MostNodeState,
    (int, status),
    (NodeSet, nodes),
    (NodeSet, votes),
    (ProposalVotes, proposalVotes),
    (ProposalSet, proposals),
    (ProposalSet, committed));

using MostNodes = FlatMap<NodeId, MostNodeState>;
using MostVoteMessages = FlatSet<MostVoteMsg>;
using MostCommitMessages = FlatSet<MostCommitMsg>;

struct_fields(MostState,
    (NodeSet, alive),
    (ProposalSet, proposed),
    (MostNodes, local),
    (MostVoteMessages, voteMsgs),
    (MostCommitMessages, commitMsgs));

MostVoteMessages broadcastVote(const MostVoteMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& proposals,
    const NodeSet& nodes) {
  auto result = messages;
  for (auto&& to : alive) {
    if (to != from) {
      result.insert(MostVoteMsg{from, to, proposals, nodes});
    }
  }
  return result;
}

MostCommitMessages broadcastCommit(const MostCommitMessages& messages,
    const NodeSet& alive,
    NodeId from,
    const ProposalSet& commit) {
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

MostState commit(MostState state, NodeId node) {
  auto& self = state.local[node];
  if (self.status == kCommitted) {
    return state;
  }
  self.status = kCommitted;
  self.committed = self.proposals;
  state.commitMsgs = broadcastCommit(state.commitMsgs, state.alive, node, self.committed);
  return state;
}

NodeSet proposalVotesFor(const ProposalVotes& proposalVotes, MessageId id) {
  for (auto&& entry : proposalVotes) {
    if (entry.id == id) {
      return entry.votes;
    }
  }
  return {};
}

ProposalVotes putProposalVotes(ProposalVotes proposalVotes,
    MessageId id,
    const NodeSet& votes) {
  for (auto it = proposalVotes.begin(); it != proposalVotes.end(); ++it) {
    if (it->id == id) {
      proposalVotes.erase(it);
      break;
    }
  }
  proposalVotes.insert(MostProposalVote{id, votes});
  return proposalVotes;
}

bool mayCommit(const MostNodeState& state) {
  for (auto&& entry : state.proposalVotes) {
    auto id = entry.id;
    auto&& votes = entry.votes;
    if (!state.proposals.contains(id)) {
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
    const ProposalSet& proposals,
    const NodeSet& incomingNodes) {
  if (state.status == kCommitted || !state.nodes.contains(source)) {
    return {false, false, false, state};
  }

  auto changedNodes = state.nodes != incomingNodes;
  auto firstVote = state.votes.empty();
  auto nodes = setIntersection(state.nodes, incomingNodes);
  auto newProposals = setUnion(state.proposals, proposals);
  auto votes = state.votes;
  votes.insert(self);
  votes.insert(source);
  votes = setIntersection(votes, nodes);

  auto proposalVotes = state.proposalVotes;
  for (auto&& id : newProposals) {
    auto votesFor = proposalVotesFor(proposalVotes, id);
    votesFor.insert(self);
    if (proposals.contains(id)) {
      votesFor.insert(source);
    }
    proposalVotes = putProposalVotes(std::move(proposalVotes), id, votesFor);
  }
  if (changedNodes) {
    ProposalVotes filteredProposalVotes;
    for (auto&& entry : proposalVotes) {
      filteredProposalVotes.insert(
          MostProposalVote{entry.id, setIntersection(entry.votes, nodes)});
    }
    proposalVotes = std::move(filteredProposalVotes);
  }

  auto local =
      MostNodeState{kVoting, nodes, votes, proposalVotes, newProposals, state.committed};
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

bool canPropose(const MostState& state, NodeId node, MessageId id) {
  return state.alive.contains(node) &&
         !state.proposed.contains(id) &&
         state.local.at(node).votes.empty() &&
         state.local.at(node).status != kCommitted;
}

MostState propose(MostState state, NodeId node, MessageId id) {
  state.proposed.insert(id);
  auto nodes = state.local.at(node).nodes;
  auto out = processVote(state.local.at(node), node, node, ProposalSet{id}, nodes);
  if (!out.changed) {
    return state;
  }
  state.local[node] = out.local;
  if (out.sendCommit) {
    return commit(std::move(state), node);
  }
  if (out.sendVote) {
    state.voteMsgs = broadcastVote(
        state.voteMsgs, state.alive, node, out.local.proposals, out.local.nodes);
  }
  return state;
}

bool canDeliverVote(const MostState& state, const MostVoteMsg& msg) {
  return state.alive.contains(msg.to);
}

MostState deliverVote(MostState state, const MostVoteMsg& msg) {
  state.voteMsgs.erase(msg);
  auto out =
      processVote(state.local.at(msg.to), msg.to, msg.from, msg.proposals, msg.nodes);
  if (!out.changed) {
    return state;
  }
  state.local[msg.to] = out.local;
  if (out.sendCommit) {
    return commit(std::move(state), msg.to);
  }
  if (out.sendVote) {
    state.voteMsgs = broadcastVote(
        state.voteMsgs, state.alive, msg.to, out.local.proposals, out.local.nodes);
  }
  return state;
}

bool canDeliverCommit(const MostState& state, const MostCommitMsg& msg) {
  return state.alive.contains(msg.to) &&
         state.local.at(msg.to).status != kCommitted &&
         state.local.at(msg.to).proposals == msg.commit;
}

MostState deliverCommit(MostState state, const MostCommitMsg& msg) {
  state.commitMsgs.erase(msg);
  auto& self = state.local[msg.to];
  self.status = kCommitted;
  self.proposals = msg.commit;
  self.committed = msg.commit;
  state.commitMsgs = broadcastCommit(state.commitMsgs, state.alive, msg.to, msg.commit);
  return state;
}

MostState disconnect(MostState state, NodeId failed) {
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
      state = commit(std::move(state), node);
    } else if (out.sendVote) {
      state.voteMsgs = broadcastVote(
          state.voteMsgs, state.alive, node, out.local.proposals, out.local.nodes);
    }
  }

  return state;
}

bool invariant(const MostState& state) {
  if (!queueEndpointsAreAlive(state.voteMsgs, state.alive) ||
      !queueEndpointsAreAlive(state.commitMsgs, state.alive)) {
    return false;
  }

  NodeSet allNodes;
  for (auto&& [node, _] : state.local) {
    allNodes.insert(node);
  }

  for (auto&& node : state.alive) {
    const auto& self = state.local.at(node);
    if (!isSubset(self.proposals, state.proposed) || !isSubset(self.votes, self.nodes)) {
      return false;
    }
    for (auto&& entry : self.proposalVotes) {
      if (!self.proposals.contains(entry.id) || !isSubset(entry.votes, allNodes)) {
        return false;
      }
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

bool commitHappened(const MostState& state) {
  return commitHappenedWithStatus(state, kCommitted);
}

DEFINE_ALGORITHM(canProposeExpr, ::leaderless_consensus::most::canPropose)
DEFINE_ALGORITHM(proposeExpr, ::leaderless_consensus::most::propose)
DEFINE_ALGORITHM(canDeliverVoteExpr, ::leaderless_consensus::most::canDeliverVote)
DEFINE_ALGORITHM(deliverVoteExpr, ::leaderless_consensus::most::deliverVote)
DEFINE_ALGORITHM(canDeliverCommitExpr, ::leaderless_consensus::most::canDeliverCommit)
DEFINE_ALGORITHM(deliverCommitExpr, ::leaderless_consensus::most::deliverCommit)
DEFINE_ALGORITHM(canDisconnectExpr, ::leaderless_consensus::canDisconnect<MostState>)
DEFINE_ALGORITHM(canLiveDisconnectExpr,
    ::leaderless_consensus::canLiveDisconnect<MostState>)
DEFINE_ALGORITHM(disconnectExpr, ::leaderless_consensus::most::disconnect)
DEFINE_ALGORITHM(invariantExpr, ::leaderless_consensus::most::invariant)
DEFINE_ALGORITHM(commitHappenedExpr, ::leaderless_consensus::most::commitHappened)

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

  Var<MostState> state{"state"};

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

TEST_F(EngineFixture, MostSafetyHoldsInvariant) {
  e.createModel<SafetyModel>();
  EXPECT_NO_THROW(e.run());
}

TEST_F(EngineFixture, MostLivenessCommitsWithMajorityAlive) {
  e.createModel<LivenessModel>();
  EXPECT_NO_THROW(e.run());
}

}  // namespace leaderless_consensus::most
