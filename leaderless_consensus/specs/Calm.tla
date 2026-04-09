-------------------------------- MODULE Calm ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

CalmToVote == "ToVote"
CalmMayCommit == "MayCommit"
CalmCannotCommit == "CannotCommit"
CalmCompleted == "Completed"

NodeState ==
  [status : {CalmToVote, CalmMayCommit, CalmCannotCommit, CalmCompleted},
   nodes : SUBSET Nodes,
   voted : SUBSET Nodes,
   carries : SUBSET MessageIds,
   committed : SUBSET MessageIds]

Vote(from, to, carries, nodes) ==
  [from |-> from, to |-> to, carries |-> carries, nodes |-> nodes]

Commit(from, to, commit) ==
  [from |-> from, to |-> to, commit |-> commit]

VoteMessage ==
  [from : Nodes, to : Nodes, carries : SUBSET MessageIds, nodes : SUBSET Nodes]

CommitMessage ==
  [from : Nodes, to : Nodes, commit : SUBSET MessageIds]

VARIABLES alive, applied, local, voteMsgs, commitMsgs

vars == <<alive, applied, local, voteMsgs, commitMsgs>>

InitLocal ==
  [n \in Nodes |->
    [status |-> CalmToVote,
     nodes |-> Nodes,
     voted |-> {},
     carries |-> {},
     committed |-> {}]]

Init ==
  /\ alive = Nodes
  /\ applied = {}
  /\ local = InitLocal
  /\ voteMsgs = {}
  /\ commitMsgs = {}

BroadcastVote(queue, from, carries, nodes, aliveSet) ==
  queue \cup {Vote(from, to, carries, nodes) : to \in (aliveSet \ {from})}

BroadcastCommit(queue, from, commit, aliveSet) ==
  queue \cup {Commit(from, to, commit) : to \in (aliveSet \ {from})}

CalmVoteResult(state, self, source, carries, incomingNodes) ==
  IF state.status = CalmCompleted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE,
        commit |-> {}]
  ELSE
    LET status0 ==
          IF state.status = CalmMayCommit /\ state.carries # carries
          THEN CalmCannotCommit
          ELSE state.status
        carries1 == state.carries \cup carries
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
            !.carries = carries1]
    IN
      IF voted1 = nodes1
      THEN
        IF status1 = CalmMayCommit
        THEN [changed |-> TRUE,
              local |-> [local1 EXCEPT
                           !.status = CalmCompleted,
                           !.carries = carries1,
                           !.committed = carries1],
              sendVote |-> FALSE,
              sendCommit |-> TRUE,
              commit |-> carries1]
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

Apply(node, msg) ==
  /\ node \in alive
  /\ msg \notin applied
  /\ local[node].voted = {}
  /\ local[node].status # CalmCompleted
  /\ LET out == CalmVoteResult(local[node], node, node, {msg}, local[node].nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied \cup {msg}
       /\ local' = [local EXCEPT ![node] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.carries, out.local.nodes,
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
           CalmVoteResult(local[msg.to], msg.to, msg.from, msg.carries, msg.nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied
       /\ local' = [local EXCEPT ![msg.to] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.carries,
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
  /\ local[msg.to].carries = msg.commit
  /\ alive' = alive
  /\ applied' = applied
  /\ local' =
       [local EXCEPT
         ![msg.to].status = CalmCompleted,
         ![msg.to].carries = msg.commit,
         ![msg.to].committed = msg.commit]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, msg.commit, alive)

DisconnectLocal(state, self, failed) ==
  IF state.carries = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          CalmVoteResult(state, self, failed, state.carries, state.nodes \ {failed})
    IN out.local

Disconnect(failed) ==
  /\ failed \in alive
  /\ alive' = alive \ {failed}
  /\ applied' = applied
  /\ local' =
       [n \in Nodes |->
         IF n \in alive'
         THEN DisconnectLocal(local[n], n, failed)
         ELSE local[n]]
  /\ voteMsgs' = {m \in voteMsgs : m.from # failed /\ m.to # failed}
  /\ commitMsgs' = {m \in commitMsgs : m.from # failed /\ m.to # failed}

Next ==
  \/ \E node \in Nodes : \E msg \in MessageIds : Apply(node, msg)
  \/ \E msg \in voteMsgs : DeliverVote(msg)
  \/ \E msg \in commitMsgs : DeliverCommit(msg)
  \/ \E failed \in Nodes : Disconnect(failed)

TypeOK ==
  /\ alive \subseteq Nodes
  /\ applied \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ voteMsgs \subseteq VoteMessage
  /\ commitMsgs \subseteq CommitMessage

LocalWellFormed ==
  \A n \in alive :
    /\ local[n].voted \subseteq local[n].nodes
    /\ local[n].carries \subseteq applied
    /\ IF local[n].status = CalmCompleted
       THEN local[n].committed = local[n].carries
       ELSE local[n].committed = {}

VoteWellFormed ==
  \A msg \in voteMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.carries \subseteq applied

CommitWellFormed ==
  \A msg \in commitMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.commit \subseteq applied

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

CanApplyAny ==
  \E node \in Nodes :
    \E msg \in MessageIds :
      /\ node \in alive
      /\ msg \notin applied
      /\ local[node].voted = {}
      /\ local[node].status # CalmCompleted

Quiescent ==
  /\ voteMsgs = {}
  /\ commitMsgs = {}
  /\ ~CanApplyAny

Termination == <>Quiescent

Spec == Init /\ [][Next]_vars
LiveSpec == Spec /\ WF_vars(Next)

=============================================================================
