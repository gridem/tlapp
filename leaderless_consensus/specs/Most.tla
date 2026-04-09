-------------------------------- MODULE Most ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

MostVoting == "Voting"
MostCommitted == "Committed"

CarryVotes == [MessageIds -> SUBSET Nodes]

NodeState ==
  [status : {MostVoting, MostCommitted},
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes,
   carryVotes : CarryVotes,
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

InitCarryVotes == [m \in MessageIds |-> {}]

InitLocal ==
  [n \in Nodes |->
    [status |-> MostVoting,
     nodes |-> Nodes,
     votes |-> {},
     carryVotes |-> InitCarryVotes,
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

MayCommit(state) ==
  \A msg \in state.carries :
    2 * Cardinality(state.carryVotes[msg]) > Cardinality(state.nodes)

MostVoteResult(state, self, source, carries, incomingNodes) ==
  IF state.status = MostCommitted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE]
  ELSE
    LET changedNodes == state.nodes # incomingNodes
        firstVote == state.votes = {}
        nodes1 == state.nodes \cap incomingNodes
        carries1 == state.carries \cup carries
        votes1 == (state.votes \cup {self, source}) \cap nodes1
        carryVotes1 ==
          [m \in MessageIds |->
             IF m \in carries
             THEN state.carryVotes[m] \cup {source}
             ELSE state.carryVotes[m]]
        carryVotes2 ==
          IF changedNodes
          THEN [m \in MessageIds |-> carryVotes1[m] \cap nodes1]
          ELSE carryVotes1
        local1 ==
          [state EXCEPT
            !.nodes = nodes1,
            !.votes = votes1,
            !.carryVotes = carryVotes2,
            !.carries = carries1]
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

Apply(node, msg) ==
  /\ node \in alive
  /\ msg \notin applied
  /\ local[node].votes = {}
  /\ local[node].status # MostCommitted
  /\ LET out == MostVoteResult(local[node], node, node, {msg}, local[node].nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied \cup {msg}
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![node].status = MostCommitted,
                    ![node].nodes = out.local.nodes,
                    ![node].votes = out.local.votes,
                    ![node].carryVotes = out.local.carryVotes,
                    ![node].carries = out.local.carries,
                    ![node].committed = out.local.carries]
            ELSE [local EXCEPT ![node] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.carries, out.local.nodes,
                               alive)
            ELSE voteMsgs
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, node, out.local.carries, alive)
            ELSE commitMsgs

DeliverVote(msg) ==
  /\ msg \in voteMsgs
  /\ msg.to \in alive
  /\ LET out ==
           MostVoteResult(local[msg.to], msg.to, msg.from, msg.carries, msg.nodes)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied
       /\ local' =
            IF out.sendCommit
            THEN [local EXCEPT
                    ![msg.to].status = MostCommitted,
                    ![msg.to].nodes = out.local.nodes,
                    ![msg.to].votes = out.local.votes,
                    ![msg.to].carryVotes = out.local.carryVotes,
                    ![msg.to].carries = out.local.carries,
                    ![msg.to].committed = out.local.carries]
            ELSE [local EXCEPT ![msg.to] = out.local]
       /\ voteMsgs' =
            IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.carries,
                               out.local.nodes, alive)
            ELSE voteMsgs \ {msg}
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, msg.to, out.local.carries, alive)
            ELSE commitMsgs

DeliverCommit(msg) ==
  /\ msg \in commitMsgs
  /\ msg.to \in alive
  /\ local[msg.to].status # MostCommitted
  /\ local[msg.to].carries = msg.commit
  /\ alive' = alive
  /\ applied' = applied
  /\ local' =
       [local EXCEPT
         ![msg.to].status = MostCommitted,
         ![msg.to].carries = msg.commit,
         ![msg.to].committed = msg.commit]
  /\ voteMsgs' = voteMsgs
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg.to, msg.commit, alive)

DisconnectLocal(state, self, failed) ==
  IF state.carries = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          MostVoteResult(state, self, failed, state.carries, state.nodes \ {failed})
    IN
      IF out.sendCommit
      THEN [out.local EXCEPT
              !.status = MostCommitted,
              !.committed = out.local.carries]
      ELSE out.local

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
    /\ local[n].votes \subseteq local[n].nodes
    /\ local[n].carries \subseteq applied
    /\ \A msg \in MessageIds : local[n].carryVotes[msg] \subseteq Nodes
    /\ IF local[n].status = MostCommitted
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
      /\ local[left].status = MostCommitted
      /\ local[right].status = MostCommitted
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
      /\ local[node].votes = {}
      /\ local[node].status # MostCommitted

Quiescent ==
  /\ voteMsgs = {}
  /\ commitMsgs = {}
  /\ ~CanApplyAny

Termination == <>Quiescent

Spec == Init /\ [][Next]_vars
LiveSpec == Spec /\ WF_vars(Next)

=============================================================================
