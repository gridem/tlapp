-------------------------------- MODULE Calm ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

Majority == (Cardinality(Nodes) \div 2) + 1

CalmToVote == "ToVote"
CalmMayCommit == "MayCommit"
CalmCannotCommit == "CannotCommit"
CalmCompleted == "Completed"

NodeState ==
  [status : {CalmToVote, CalmMayCommit, CalmCannotCommit, CalmCompleted},
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
    [status |-> CalmToVote,
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

CalmVoteResult(state, self, source, proposals, incomingNodes) ==
  IF state.status = CalmCompleted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE,
        commit |-> {}]
  ELSE
    LET status0 ==
          IF state.status = CalmMayCommit /\ state.proposals # proposals
          THEN CalmCannotCommit
          ELSE state.status
        proposals1 == state.proposals \cup proposals
        voted0 == state.voted \cup {source, self}
        nodes1 ==
          IF state.nodes # incomingNodes
          THEN state.nodes \cap incomingNodes
          ELSE state.nodes
        voted1 ==
          IF state.nodes # incomingNodes
          THEN voted0 \cap incomingNodes
          ELSE voted0
        status1 ==
          IF state.nodes # incomingNodes /\ status0 = CalmMayCommit
          THEN CalmCannotCommit
          ELSE status0
        local1 ==
          [state EXCEPT
            !.status = status1,
            !.nodes = nodes1,
            !.voted = voted1,
            !.proposals = proposals1]
    IN
      IF voted1 = nodes1
      THEN
        IF status1 = CalmMayCommit
        THEN [changed |-> TRUE,
              local |-> [local1 EXCEPT
                           !.status = CalmCompleted,
                           !.proposals = proposals1,
                           !.committed = proposals1],
              sendVote |-> FALSE,
              sendCommit |-> TRUE,
              commit |-> proposals1]
        ELSE [changed |-> TRUE,
              local |-> [local1 EXCEPT !.status = CalmMayCommit],
              sendVote |-> TRUE,
              sendCommit |-> FALSE,
              commit |-> {}]
      ELSE
        IF status1 = CalmToVote
        THEN [changed |-> TRUE,
              local |-> [local1 EXCEPT !.status = CalmMayCommit],
              sendVote |-> TRUE,
              sendCommit |-> FALSE,
              commit |-> {}]
        ELSE [changed |-> TRUE,
              local |-> local1,
              sendVote |-> FALSE,
              sendCommit |-> FALSE,
              commit |-> {}]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ local[node].voted = {}
  /\ local[node].status # CalmCompleted
  /\ LET out == CalmVoteResult(local[node], node, node, {msg}, local[node].nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' = [local EXCEPT ![node] = out.local]
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
           CalmVoteResult(local[msg.to], msg.to, msg.from, msg.proposals, msg.nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' = [local EXCEPT ![msg.to] = out.local]
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
  /\ local[msg.to].status # CalmCompleted
  /\ local[msg.to].proposals = msg.commit
  /\ alive' = alive
  /\ proposed' = proposed
  /\ local' =
       [local EXCEPT
         ![msg.to].status = CalmCompleted,
         ![msg.to].proposals = msg.commit,
         ![msg.to].committed = msg.commit]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, msg.commit, alive)

DisconnectLocal(state, self, failed) ==
  IF state.proposals = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          CalmVoteResult(state, self, failed, state.proposals, state.nodes \ {failed})
    IN out.local

Disconnect(failed) ==
  /\ failed \in alive
  /\ alive' = alive \ {failed}
  /\ proposed' = proposed
  /\ local' =
       [n \in Nodes |->
         IF n \in alive'
         THEN DisconnectLocal(local[n], n, failed)
         ELSE local[n]]
  /\ voteMsgs' = {m \in voteMsgs : m.from # failed /\ m.to # failed}
  /\ commitMsgs' = {m \in commitMsgs : m.from # failed /\ m.to # failed}

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
    /\ local[n].voted \subseteq local[n].nodes
    /\ local[n].proposals \subseteq proposed
    /\ IF local[n].status = CalmCompleted
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
      /\ local[left].status = CalmCompleted
      /\ local[right].status = CalmCompleted
      => local[left].committed = local[right].committed

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ VoteWellFormed
  /\ CommitWellFormed
  /\ Agreement

CommitHappened ==
  \E node \in Nodes :
    /\ local[node].status = CalmCompleted
    /\ local[node].committed # {}

Termination == <>CommitHappened

Spec == Init /\ [][Next]_vars
LiveSpec ==
  /\ Init /\ [][LiveNext]_vars
  /\ WF_vars(ProposeAny)
  /\ WF_vars(DeliverAnyVote)

=============================================================================
