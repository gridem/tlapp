-------------------------------- MODULE Sore ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

SoreInitial == "Initial"
SoreVoted == "Voted"
SoreCompleted == "Completed"

NodeState ==
  [status : {SoreInitial, SoreVoted, SoreCompleted},
   nodes : SUBSET Nodes,
   voted : SUBSET Nodes,
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

InitLocal ==
  [n \in Nodes |->
    [status |-> SoreInitial,
     nodes |-> Nodes,
     voted |-> {},
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

SoreVoteResult(state, self, source, proposals, incomingNodes) ==
  IF state.status = SoreCompleted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE,
        commit |-> {}]
  ELSE
    LET changedNodes == state.nodes # incomingNodes
        status1 ==
          IF changedNodes
          THEN SoreInitial
          ELSE state.status
        nodes1 ==
          IF changedNodes
          THEN state.nodes \cap incomingNodes
          ELSE state.nodes
        voted0 ==
          IF changedNodes
          THEN {}
          ELSE state.voted
        voted1 == (voted0 \cup {source, self}) \cap nodes1
        proposals1 == state.proposals \cup proposals
        local1 ==
          [state EXCEPT
            !.status = status1,
            !.nodes = nodes1,
            !.voted = voted1,
            !.proposals = proposals1]
    IN
      IF voted1 = nodes1
      THEN [changed |-> local1 # state,
            local |-> local1,
            sendVote |-> FALSE,
            sendCommit |-> TRUE,
            commit |-> proposals1]
      ELSE
        IF status1 = SoreInitial
        THEN [changed |-> TRUE,
              local |-> [local1 EXCEPT !.status = SoreVoted],
              sendVote |-> TRUE,
              sendCommit |-> FALSE,
              commit |-> {}]
        ELSE [changed |-> local1 # state,
              local |-> local1,
              sendVote |-> FALSE,
              sendCommit |-> FALSE,
              commit |-> {}]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ local[node].voted = {}
  /\ local[node].status # SoreCompleted
  /\ LET out == SoreVoteResult(local[node], node, node, {msg}, local[node].nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![node].status = SoreCompleted,
                    ![node].nodes = out.local.nodes,
                    ![node].voted = out.local.voted,
                    ![node].proposals = out.commit,
                    ![node].committed = out.commit]
            ELSE [local EXCEPT ![node] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.proposals, out.local.nodes,
                               alive)
            ELSE voteMsgs
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, node, out.commit, alive)
            ELSE commitMsgs

DeliverVote(msg) ==
  /\ msg \in voteMsgs
  /\ msg.to \in alive
  /\ LET out ==
           SoreVoteResult(local[msg.to], msg.to, msg.from, msg.proposals, msg.nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![msg.to].status = SoreCompleted,
                    ![msg.to].nodes = out.local.nodes,
                    ![msg.to].voted = out.local.voted,
                    ![msg.to].proposals = out.commit,
                    ![msg.to].committed = out.commit]
            ELSE [local EXCEPT ![msg.to] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.proposals,
                               out.local.nodes, alive)
            ELSE voteMsgs \ {msg}
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, msg.to, out.commit, alive)
            ELSE commitMsgs

DeliverCommit(msg) ==
  /\ msg \in commitMsgs
  /\ msg.to \in alive
  /\ local[msg.to].status # SoreCompleted
  /\ alive' = alive
  /\ proposed' = proposed
  /\ local' =
       [local EXCEPT
         ![msg.to].status = SoreCompleted,
         ![msg.to].proposals = msg.commit,
         ![msg.to].committed = msg.commit]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, msg.commit, alive)

DisconnectLocal(state, self, failed) ==
  IF state.proposals = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          SoreVoteResult(state, self, failed, state.proposals, state.nodes \ {failed})
    IN
      IF out.sendCommit
      THEN [out.local EXCEPT
              !.status = SoreCompleted,
              !.proposals = out.commit,
              !.committed = out.commit]
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
          SoreVoteResult(state, self, failed, state.proposals, state.nodes \ {failed})
    IN
      IF out.sendCommit
      THEN [changed |-> out.changed,
            local |-> [out.local EXCEPT
                         !.status = SoreCompleted,
                         !.proposals = out.commit,
                         !.committed = out.commit],
            sendVote |-> FALSE,
            sendCommit |-> TRUE,
            commit |-> out.commit]
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

Next ==
  \/ \E node \in Nodes : \E msg \in MessageIds : Propose(node, msg)
  \/ \E msg \in voteMsgs : DeliverVote(msg)
  \/ \E msg \in commitMsgs : DeliverCommit(msg)
  \/ \E failed \in Nodes : Disconnect(failed)

TypeOK ==
  /\ alive \subseteq Nodes
  /\ proposed \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ voteMsgs \subseteq VoteMessage
  /\ commitMsgs \subseteq CommitMessage

LocalWellFormed ==
  \A n \in alive :
    /\ local[n].voted \subseteq local[n].nodes
    /\ local[n].proposals \subseteq proposed
    /\ IF local[n].status = SoreCompleted
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
      /\ local[left].status = SoreCompleted
      /\ local[right].status = SoreCompleted
      => local[left].committed = local[right].committed

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ VoteWellFormed
  /\ CommitWellFormed
  /\ Agreement

Spec == Init /\ [][Next]_vars

=============================================================================
