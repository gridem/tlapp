-------------------------------- MODULE Flat ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

FlatVoting == "Voting"
FlatCommitted == "Committed"

NodeState ==
  [status : {FlatVoting, FlatCommitted},
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes,
   carries : SUBSET MessageIds,
   committed : SUBSET MessageIds]

Vote(from, to, carries, nodes, votes) ==
  [from |-> from,
   to |-> to,
   carries |-> carries,
   nodes |-> nodes,
   votes |-> votes]

Commit(from, to) ==
  [from |-> from, to |-> to]

VoteMessage ==
  [from : Nodes,
   to : Nodes,
   carries : SUBSET MessageIds,
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes]

CommitMessage ==
  [from : Nodes, to : Nodes]

VARIABLES alive, applied, local, voteMsgs, commitMsgs

vars == <<alive, applied, local, voteMsgs, commitMsgs>>

InitLocal ==
  [n \in Nodes |->
    [status |-> FlatVoting,
     nodes |-> Nodes,
     votes |-> {},
     carries |-> {},
     committed |-> {}]]

Init ==
  /\ alive = Nodes
  /\ applied = {}
  /\ local = InitLocal
  /\ voteMsgs = {}
  /\ commitMsgs = {}

BroadcastVote(queue, from, carries, nodes, votes, aliveSet) ==
  queue \cup {Vote(from, to, carries, nodes, votes) :
                to \in (aliveSet \ {from})}

BroadcastCommit(queue, from, aliveSet) ==
  queue \cup {Commit(from, to) : to \in (aliveSet \ {from})}

FlatVoteResult(state, self, source, carries, incomingNodes, incomingVotes) ==
  IF state.status = FlatCommitted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE]
  ELSE
    LET nodes1 == state.nodes \cap incomingNodes
        carries1 == state.carries \cup carries
        votes0 == {self}
        votes1 ==
          IF nodes1 = incomingNodes /\ carries1 = carries
          THEN votes0 \cup incomingVotes
          ELSE votes0
        votes2 ==
          IF nodes1 = state.nodes /\ carries1 = state.carries
          THEN votes1 \cup state.votes
          ELSE votes1
        votes3 == votes2 \cap nodes1
        local1 ==
          [state EXCEPT
            !.nodes = nodes1,
            !.votes = votes3,
            !.carries = carries1]
    IN
      IF /\ nodes1 = state.nodes
         /\ carries1 = state.carries
         /\ votes3 = state.votes
      THEN [changed |-> FALSE,
            local |-> state,
            sendVote |-> FALSE,
            sendCommit |-> FALSE]
      ELSE
        IF nodes1 = votes3
        THEN [changed |-> TRUE,
              local |-> [local1 EXCEPT
                           !.status = FlatCommitted,
                           !.committed = carries1],
              sendVote |-> FALSE,
              sendCommit |-> TRUE]
        ELSE [changed |-> TRUE,
              local |-> local1,
              sendVote |-> TRUE,
              sendCommit |-> FALSE]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin applied
  /\ msg = node + 10
  /\ local[node].votes = {}
  /\ local[node].status # FlatCommitted
  /\ LET out ==
           FlatVoteResult(local[node], node, node, {msg}, local[node].nodes, {})
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied \cup {msg}
       /\ local' = [local EXCEPT ![node] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.carries, out.local.nodes,
                               out.local.votes, alive)
            ELSE voteMsgs
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, node, alive)
            ELSE commitMsgs

DeliverVote(msg) ==
  /\ msg \in voteMsgs
  /\ msg.to \in alive
  /\ LET out ==
           FlatVoteResult(local[msg.to], msg.to, msg.from, msg.carries, msg.nodes,
                          msg.votes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied
       /\ local' = [local EXCEPT ![msg.to] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.carries,
                               out.local.nodes, out.local.votes, alive)
            ELSE voteMsgs \ {msg}
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, msg.to, alive)
            ELSE commitMsgs

DeliverCommit(msg) ==
  /\ msg \in commitMsgs
  /\ msg.to \in alive
  /\ local[msg.to].status # FlatCommitted
  /\ alive' = alive
  /\ applied' = applied
  /\ local' =
       [local EXCEPT
         ![msg.to].status = FlatCommitted,
         ![msg.to].committed = local[msg.to].carries]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, alive)

DisconnectLocal(state, self, failed) ==
  IF state.carries = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          FlatVoteResult(state, self, failed, state.carries, state.nodes \ {failed},
                         {})
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
  \/ \E node \in Nodes : \E msg \in MessageIds : Propose(node, msg)
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
    /\ local[n].votes \subseteq local[n].nodes
    /\ local[n].carries \subseteq applied
    /\ IF local[n].status = FlatCommitted
       THEN local[n].committed = local[n].carries
       ELSE local[n].committed = {}

VoteWellFormed ==
  \A msg \in voteMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.carries \subseteq applied
    /\ msg.votes \subseteq msg.nodes

CommitWellFormed ==
  \A msg \in commitMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive

Agreement ==
  \A left \in alive :
    \A right \in alive :
      /\ local[left].status = FlatCommitted
      /\ local[right].status = FlatCommitted
      => local[left].committed = local[right].committed

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ VoteWellFormed
  /\ CommitWellFormed
  /\ Agreement

Spec == Init /\ [][Next]_vars

=============================================================================
