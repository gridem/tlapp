-------------------------------- MODULE Most ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

Majority == (Cardinality(Nodes) \div 2) + 1

MostVoting == "Voting"
MostCommitted == "Committed"

ProposalVotes == [MessageIds -> SUBSET Nodes]

NodeState ==
  [status : {MostVoting, MostCommitted},
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes,
   proposalVotes : ProposalVotes,
   proposals : SUBSET MessageIds,
   committed : SUBSET MessageIds]

Vote(from, to, proposals, nodes) ==
  [from |-> from, to |-> to, proposals |-> proposals, nodes |-> nodes]

Commit(from, to, commit) ==
  [from |-> from, to |-> to, commit |-> commit]

VoteMessage ==
  [from : Nodes, to : Nodes, proposals : SUBSET MessageIds, nodes : SUBSET Nodes]

CommitMessage ==
  [from : Nodes, to : Nodes, commit : SUBSET MessageIds]

VARIABLES alive, proposed, local, voteMsgs, commitMsgs

vars == <<alive, proposed, local, voteMsgs, commitMsgs>>

InitProposalVotes == [m \in MessageIds |-> {}]

InitLocal ==
  [n \in Nodes |->
    [status |-> MostVoting,
     nodes |-> Nodes,
     votes |-> {},
     proposalVotes |-> InitProposalVotes,
     proposals |-> {},
     committed |-> {}]]

Init ==
  /\ alive = Nodes
  /\ proposed = {}
  /\ local = InitLocal
  /\ voteMsgs = {}
  /\ commitMsgs = {}

BroadcastVote(queue, from, proposals, nodes, aliveSet) ==
  queue \cup {Vote(from, to, proposals, nodes) : to \in (aliveSet \ {from})}

BroadcastCommit(queue, from, commit, aliveSet) ==
  queue \cup {Commit(from, to, commit) : to \in (aliveSet \ {from})}

MayCommit(state) ==
  \A msg \in state.proposals :
    2 * Cardinality(state.proposalVotes[msg]) > Cardinality(state.nodes)

MostVoteResult(state, self, source, proposals, incomingNodes) ==
  IF state.status = MostCommitted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE]
  ELSE
    LET changedNodes == state.nodes # incomingNodes
        firstVote == state.votes = {}
        nodes1 == state.nodes \cap incomingNodes
        proposals1 == state.proposals \cup proposals
        votes1 == (state.votes \cup {self, source}) \cap nodes1
        proposalVotes1 ==
          [m \in MessageIds |->
             IF m \in proposals1
             THEN state.proposalVotes[m] \cup {self} \cup
                  (IF m \in proposals THEN {source} ELSE {})
             ELSE state.proposalVotes[m]]
        proposalVotes2 ==
          IF changedNodes
          THEN [m \in MessageIds |-> proposalVotes1[m] \cap nodes1]
          ELSE proposalVotes1
        local1 ==
          [state EXCEPT
            !.nodes = nodes1,
            !.votes = votes1,
            !.proposalVotes = proposalVotes2,
            !.proposals = proposals1]
        local2 ==
          IF changedNodes
          THEN [local1 EXCEPT !.votes = {self}]
          ELSE local1
    IN
      IF firstVote
      THEN [changed |-> local1 # state,
            local |-> local1,
            sendVote |-> TRUE,
            sendCommit |-> FALSE]
      ELSE
        IF local2.votes # local2.nodes
        THEN [changed |-> local2 # state,
              local |-> local2,
              sendVote |-> changedNodes,
              sendCommit |-> FALSE]
        ELSE
          IF MayCommit(local2)
          THEN [changed |-> local2 # state,
                local |-> local2,
                sendVote |-> FALSE,
                sendCommit |-> TRUE]
          ELSE [changed |-> local2 # state,
                local |-> local2,
                sendVote |-> TRUE,
                sendCommit |-> FALSE]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ local[node].votes = {}
  /\ local[node].status # MostCommitted
  /\ LET out == MostVoteResult(local[node], node, node, {msg}, local[node].nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![node].status = MostCommitted,
                    ![node].nodes = out.local.nodes,
                    ![node].votes = out.local.votes,
                    ![node].proposalVotes = out.local.proposalVotes,
                    ![node].proposals = out.local.proposals,
                    ![node].committed = out.local.proposals]
            ELSE [local EXCEPT ![node] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.proposals, out.local.nodes,
                               alive)
            ELSE voteMsgs
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, node, out.local.proposals, alive)
            ELSE commitMsgs

DeliverVote(msg) ==
  /\ msg \in voteMsgs
  /\ msg.to \in alive
  /\ LET out ==
           MostVoteResult(local[msg.to], msg.to, msg.from, msg.proposals, msg.nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![msg.to].status = MostCommitted,
                    ![msg.to].nodes = out.local.nodes,
                    ![msg.to].votes = out.local.votes,
                    ![msg.to].proposalVotes = out.local.proposalVotes,
                    ![msg.to].proposals = out.local.proposals,
                    ![msg.to].committed = out.local.proposals]
            ELSE [local EXCEPT ![msg.to] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.proposals,
                               out.local.nodes, alive)
            ELSE voteMsgs \ {msg}
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, msg.to, out.local.proposals, alive)
            ELSE commitMsgs

DeliverCommit(msg) ==
  /\ msg \in commitMsgs
  /\ msg.to \in alive
  /\ local[msg.to].status # MostCommitted
  /\ local[msg.to].proposals = msg.commit
  /\ alive' = alive
  /\ proposed' = proposed
  /\ local' =
       [local EXCEPT
         ![msg.to].status = MostCommitted,
         ![msg.to].proposals = msg.commit,
         ![msg.to].committed = msg.commit]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, msg.commit, alive)

DisconnectLocal(state, self, failed) ==
  IF state.proposals = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          MostVoteResult(state, self, failed, state.proposals, state.nodes \ {failed})
    IN
      IF out.sendCommit
      THEN [out.local EXCEPT
              !.status = MostCommitted,
              !.committed = out.local.proposals]
      ELSE out.local

DisconnectOut(state, self, failed) ==
  IF state.proposals = {}
  THEN [changed |-> TRUE,
        local |-> [state EXCEPT !.nodes = @ \ {failed}],
        sendVote |-> FALSE,
        sendCommit |-> FALSE,
        commit |-> {}]
  ELSE
    LET out ==
          MostVoteResult(state, self, failed, state.proposals, state.nodes \ {failed})
    IN
      IF out.sendCommit
      THEN [changed |-> out.changed,
            local |-> [out.local EXCEPT
                         !.status = MostCommitted,
                         !.committed = out.local.proposals],
            sendVote |-> FALSE,
            sendCommit |-> TRUE,
            commit |-> out.local.proposals]
      ELSE [changed |-> out.changed,
            local |-> out.local,
            sendVote |-> out.sendVote,
            sendCommit |-> FALSE,
            commit |-> {}]

RECURSIVE DisconnectBroadcastVotes(_, _, _, _)

DisconnectBroadcastVotes(queue, senders, localAfter, aliveSet) ==
  IF senders = {}
  THEN queue
  ELSE LET sender == CHOOSE n \in senders : TRUE
       IN DisconnectBroadcastVotes(
            BroadcastVote(queue, sender, localAfter[sender].proposals,
                          localAfter[sender].nodes, aliveSet),
            senders \ {sender},
            localAfter,
            aliveSet)

RECURSIVE DisconnectBroadcastCommits(_, _, _, _)

DisconnectBroadcastCommits(queue, senders, localAfter, aliveSet) ==
  IF senders = {}
  THEN queue
  ELSE LET sender == CHOOSE n \in senders : TRUE
       IN DisconnectBroadcastCommits(
            BroadcastCommit(queue, sender, localAfter[sender].committed, aliveSet),
            senders \ {sender},
            localAfter,
            aliveSet)

Disconnect(failed) ==
  /\ failed \in alive
  /\ LET alive1 == alive \ {failed}
         out ==
           [n \in Nodes |->
             IF n \in alive1
             THEN DisconnectOut(local[n], n, failed)
             ELSE [changed |-> FALSE,
                   local |-> local[n],
                   sendVote |-> FALSE,
                   sendCommit |-> FALSE,
                   commit |-> {}]]
         local1 ==
           [n \in Nodes |->
             IF n \in alive1
             THEN out[n].local
             ELSE local[n]]
         voteBase ==
           {m \in voteMsgs : m.from # failed /\ m.to # failed}
         commitBase ==
           {m \in commitMsgs : m.from # failed /\ m.to # failed}
         voteOut ==
           DisconnectBroadcastVotes(voteBase,
               {n \in alive1 : out[n].sendVote},
               local1,
               alive1)
         commitOut ==
           DisconnectBroadcastCommits(commitBase,
               {n \in alive1 : out[n].sendCommit},
               local1,
               alive1)
     IN
       /\ alive' = alive1
       /\ proposed' = proposed
       /\ local' = local1
       /\ voteMsgs' = voteOut
       /\ commitMsgs' = commitOut

ProposeAny ==
  \E node \in Nodes : \E msg \in MessageIds : Propose(node, msg)

DeliverAnyVote ==
  \E msg \in voteMsgs : DeliverVote(msg)

DeliverAnyCommit ==
  \E msg \in commitMsgs : DeliverCommit(msg)

DisconnectAny ==
  \E failed \in Nodes : Disconnect(failed)

LiveDisconnect(failed) ==
  /\ failed \in alive
  /\ Cardinality(alive \ {failed}) >= Majority
  /\ Disconnect(failed)

LiveDisconnectAny ==
  \E failed \in Nodes : LiveDisconnect(failed)

Next ==
  \/ ProposeAny
  \/ DeliverAnyVote
  \/ DeliverAnyCommit
  \/ DisconnectAny

LiveNext ==
  \/ ProposeAny
  \/ DeliverAnyVote
  \/ DeliverAnyCommit
  \/ LiveDisconnectAny

TypeOK ==
  /\ alive \subseteq Nodes
  /\ proposed \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ voteMsgs \subseteq VoteMessage
  /\ commitMsgs \subseteq CommitMessage

LocalWellFormed ==
  \A n \in alive :
    /\ local[n].votes \subseteq local[n].nodes
    /\ local[n].proposals \subseteq proposed
    /\ \A msg \in MessageIds : local[n].proposalVotes[msg] \subseteq Nodes
    /\ IF local[n].status = MostCommitted
       THEN local[n].committed = local[n].proposals
       ELSE local[n].committed = {}

VoteWellFormed ==
  \A msg \in voteMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.proposals \subseteq proposed

CommitWellFormed ==
  \A msg \in commitMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.commit \subseteq proposed

Agreement ==
  \A left \in alive :
    \A right \in alive :
      /\ local[left].status = MostCommitted
      /\ local[right].status = MostCommitted
      => local[left].committed = local[right].committed

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ VoteWellFormed
  /\ CommitWellFormed
  /\ Agreement

CommitHappened ==
  \E node \in Nodes :
    /\ local[node].status = MostCommitted
    /\ local[node].committed # {}

Termination == <>CommitHappened

Spec == Init /\ [][Next]_vars
LiveSpec ==
  /\ Init /\ [][LiveNext]_vars
  /\ WF_vars(ProposeAny)
  /\ WF_vars(DeliverAnyVote)

=============================================================================
