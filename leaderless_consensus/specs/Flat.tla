-------------------------------- MODULE Flat ---------------------------------
EXTENDS Integers, FiniteSets, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

Majority == (Cardinality(Nodes) \div 2) + 1

FlatVoting == "Voting"
FlatCommitted == "Committed"

NodeState ==
  [status : {FlatVoting, FlatCommitted},
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes,
   proposals : SUBSET MessageIds,
   committed : SUBSET MessageIds]

Vote(from, to, proposals, nodes, votes) ==
  [from |-> from,
   to |-> to,
   proposals |-> proposals,
   nodes |-> nodes,
   votes |-> votes]

Commit(from, to) ==
  [from |-> from, to |-> to]

VoteMessage ==
  [from : Nodes,
   to : Nodes,
   proposals : SUBSET MessageIds,
   nodes : SUBSET Nodes,
   votes : SUBSET Nodes]

VARIABLES alive, proposed, local, voteMsgs, commitMsgs

vars == <<alive, proposed, local, voteMsgs, commitMsgs>>

InitLocal ==
  [n \in Nodes |->
    [status |-> FlatVoting,
     nodes |-> Nodes,
     votes |-> {},
     proposals |-> {},
     committed |-> {}]]

Init ==
  /\ alive = Nodes
  /\ proposed = {}
  /\ local = InitLocal
  /\ voteMsgs = {}
  /\ commitMsgs = {}

BroadcastVote(queue, from, proposals, nodes, votes, aliveSet) ==
  {m \in queue :
     ~(\E to \in (aliveSet \ {from}) :
         /\ m.from = from
         /\ m.to = to
         /\ m.proposals = proposals
         /\ m.nodes = nodes
         /\ m.votes \subseteq votes)} \cup
  {Vote(from, to, proposals, nodes, votes) :
     /\ to \in (aliveSet \ {from})
     /\ ~(\E m \in queue :
            /\ m.from = from
            /\ m.to = to
            /\ m.proposals = proposals
            /\ m.nodes = nodes
            /\ votes \subseteq m.votes)}

PurgeVotesTo(queue, node) ==
  {m \in queue : m.to # node}

BroadcastCommit(queue, from, aliveSet, localState) ==
  (queue \ {from}) \cup
  {to \in (aliveSet \ {from}) : localState[to].status # FlatCommitted}

FlatVoteResult(state, self, source, proposals, incomingNodes, incomingVotes) ==
  IF state.status = FlatCommitted \/ source \notin state.nodes
  THEN [changed |-> FALSE,
        local |-> state,
        sendVote |-> FALSE,
        sendCommit |-> FALSE]
  ELSE
    LET nodes1 == state.nodes \cap incomingNodes
        proposals1 == state.proposals \cup proposals
        votes0 == {self}
        votes1 ==
          IF nodes1 = incomingNodes /\ proposals1 = proposals
          THEN votes0 \cup incomingVotes
          ELSE votes0
        votes2 ==
          IF nodes1 = state.nodes /\ proposals1 = state.proposals
          THEN votes1 \cup state.votes
          ELSE votes1
        votes3 == votes2 \cap nodes1
        local1 ==
          [state EXCEPT
            !.nodes = nodes1,
            !.votes = votes3,
            !.proposals = proposals1]
    IN
      IF /\ nodes1 = state.nodes
         /\ proposals1 = state.proposals
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
                           !.committed = proposals1],
              sendVote |-> FALSE,
              sendCommit |-> TRUE]
        ELSE [changed |-> TRUE,
              local |-> local1,
              sendVote |-> TRUE,
              sendCommit |-> FALSE]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ msg = node + 10
  /\ local[node].votes = {}
  /\ local[node].status # FlatCommitted
  /\ LET out ==
           FlatVoteResult(local[node], node, node, {msg}, local[node].nodes, {})
         local1 == [local EXCEPT ![node] = out.local]
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' = local1
       /\ voteMsgs' =
            IF out.sendCommit
            THEN PurgeVotesTo(voteMsgs, node)
            ELSE IF out.sendVote
            THEN BroadcastVote(voteMsgs, node, out.local.proposals, out.local.nodes,
                               out.local.votes, alive)
            ELSE voteMsgs
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, node, alive, local1)
            ELSE commitMsgs

DeliverVote(msg) ==
  /\ msg \in voteMsgs
  /\ msg.to \in alive
  /\ LET out ==
           FlatVoteResult(local[msg.to], msg.to, msg.from, msg.proposals, msg.nodes,
                          msg.votes)
         local1 == [local EXCEPT ![msg.to] = out.local]
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' = local1
       /\ voteMsgs' =
            IF out.sendCommit
            THEN PurgeVotesTo(voteMsgs \ {msg}, msg.to)
            ELSE IF out.sendVote
            THEN BroadcastVote(voteMsgs \ {msg}, msg.to, out.local.proposals,
                               out.local.nodes, out.local.votes, alive)
            ELSE voteMsgs \ {msg}
       /\ commitMsgs' =
            IF out.sendCommit
            THEN BroadcastCommit(commitMsgs, msg.to, alive, local1)
            ELSE commitMsgs

DeliverCommit(msg) ==
  /\ msg \in commitMsgs
  /\ msg \in alive
  /\ local[msg].status # FlatCommitted
  /\ LET local1 ==
           [local EXCEPT
             ![msg].status = FlatCommitted,
             ![msg].committed = local[msg].proposals]
     IN
  /\ alive' = alive
  /\ proposed' = proposed
  /\ local' = local1
  /\ voteMsgs' = PurgeVotesTo(voteMsgs, msg)
  /\ commitMsgs' = BroadcastCommit(commitMsgs \ {msg}, msg, alive, local1)

DisconnectLocal(state, self, failed) ==
  IF state.proposals = {}
  THEN [state EXCEPT !.nodes = @ \ {failed}]
  ELSE
    LET out ==
          FlatVoteResult(state, self, failed, state.proposals, state.nodes \ {failed},
                         {})
    IN out.local

DisconnectOut(state, self, failed) ==
  IF state.proposals = {}
  THEN [changed |-> TRUE,
        local |-> [state EXCEPT !.nodes = @ \ {failed}],
        sendVote |-> FALSE,
        sendCommit |-> FALSE]
  ELSE FlatVoteResult(state, self, failed, state.proposals, state.nodes \ {failed}, {})

RECURSIVE DisconnectBroadcast(_, _, _, _)

DisconnectBroadcast(queue, senders, localAfter, aliveSet) ==
  IF senders = {}
  THEN queue
  ELSE LET sender == CHOOSE n \in senders : TRUE
       IN DisconnectBroadcast(
            BroadcastVote(queue, sender, localAfter[sender].proposals,
                          localAfter[sender].nodes, localAfter[sender].votes,
                          aliveSet),
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
                   sendCommit |-> FALSE]]
         local1 ==
           [n \in Nodes |->
             IF n \in alive1
             THEN out[n].local
             ELSE local[n]]
         committedNow == {n \in alive1 : out[n].sendCommit}
         local2 ==
           [n \in Nodes |->
             IF n \in committedNow
             THEN [local1[n] EXCEPT
                     !.status = FlatCommitted,
                     !.committed = local1[n].proposals]
             ELSE local1[n]]
         voteBase ==
           {m \in voteMsgs :
              /\ m.from # failed
              /\ m.to # failed
              /\ m.to \notin committedNow}
         voteOut ==
           DisconnectBroadcast(voteBase,
               {n \in alive1 : out[n].sendVote},
               local2,
               alive1)
         commitOut ==
           IF committedNow = {}
           THEN (commitMsgs \ {failed})
           ELSE alive1 \ committedNow
     IN
       /\ alive' = alive1
       /\ proposed' = proposed
       /\ local' = local2
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
  /\ commitMsgs \subseteq Nodes

LocalWellFormed ==
  \A n \in alive :
    /\ local[n].votes \subseteq local[n].nodes
    /\ local[n].proposals \subseteq proposed
    /\ IF local[n].status = FlatCommitted
       THEN local[n].committed = local[n].proposals
       ELSE local[n].committed = {}

VoteWellFormed ==
  \A msg \in voteMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ msg.proposals \subseteq proposed
    /\ msg.votes \subseteq msg.nodes

CommitWellFormed ==
  \A msg \in commitMsgs :
    msg \in alive

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

CommitHappened ==
  \E node \in Nodes :
    /\ local[node].status = FlatCommitted
    /\ local[node].committed # {}

Termination == <>CommitHappened

Spec == Init /\ [][Next]_vars
LiveSpec ==
  /\ Init /\ [][LiveNext]_vars
  /\ WF_vars(ProposeAny)
  /\ WF_vars(DeliverAnyVote)

=============================================================================
