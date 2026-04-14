-------------------------------- MODULE Rush ---------------------------------
EXTENDS Integers, FiniteSets, Sequences, TLC

CONSTANTS Nodes, MessageIds

ASSUME Cardinality(Nodes) = 3

AllMessageSeqs ==
  UNION {[1..k -> MessageIds] : k \in 0..Cardinality(MessageIds)}

NoDuplicates(seq) ==
  \A i, j \in DOMAIN seq : i # j => seq[i] # seq[j]

MessageSeqs ==
  {seq \in AllMessageSeqs : NoDuplicates(seq)}

MaxGeneration == 2 * Cardinality(MessageIds) + 1

Majority == Cardinality(Nodes) \div 2 + 1

MajorityOf(nodes) == Cardinality(nodes) \div 2 + 1

GenerationState ==
  [messages : MessageSeqs, generation : 0..MaxGeneration]

PromiseState ==
  [prefix : MessageSeqs, votes : SUBSET Nodes]

CoreState ==
  [proposals : SUBSET MessageIds,
   nodesMessages : [Nodes -> GenerationState],
   promises : SUBSET PromiseState]

NodeState ==
  [core : CoreState, committed : MessageSeqs]

StateMessage ==
  [from : Nodes, to : Nodes, core : CoreState, committed : MessageSeqs]

VARIABLES alive, proposed, local, stateMsgs

vars == <<alive, proposed, local, stateMsgs>>

InitPromises == {}

EmptyNodesMessages ==
  [n \in Nodes |-> [messages |-> <<>>, generation |-> 0]]

EmptyCore ==
  [proposals |-> {},
   nodesMessages |-> EmptyNodesMessages,
   promises |-> InitPromises]

EmptyNodeState ==
  [core |-> EmptyCore, committed |-> <<>>]

InitLocal ==
  [n \in Nodes |-> EmptyNodeState]

Init ==
  /\ alive = Nodes
  /\ proposed = {}
  /\ local = InitLocal
  /\ stateMsgs = {}

SeqElems(seq) == {seq[i] : i \in DOMAIN seq}

IsPrefix(left, right) ==
  /\ Len(left) <= Len(right)
  /\ left = SubSeq(right, 1, Len(left))

Comparable(left, right) ==
  IsPrefix(left, right) \/ IsPrefix(right, left)

SuffixAfter(values, prefix) ==
  IF IsPrefix(prefix, values)
  THEN IF Len(prefix) = Len(values)
       THEN <<>>
       ELSE SubSeq(values, Len(prefix) + 1, Len(values))
  ELSE <<>>

RECURSIVE SortedSeqFromSet(_)

MinSet(set) ==
  CHOOSE item \in set : \A other \in set : item <= other

SortedSeqFromSet(set) ==
  IF set = {}
  THEN <<>>
  ELSE LET item == MinSet(set)
       IN <<item>> \o SortedSeqFromSet(set \ {item})

NewProposalSeq(oldProposals, incomingProposals) ==
  SortedSeqFromSet(incomingProposals \ oldProposals)

SortFrom(seq, idx) ==
  IF idx >= Len(seq)
  THEN seq
  ELSE IF idx = 1
       THEN SortedSeqFromSet(SeqElems(seq))
       ELSE SubSeq(seq, 1, idx - 1) \o
            SortedSeqFromSet(SeqElems(SubSeq(seq, idx, Len(seq))))

BroadcastState(queue, from, state, aliveSet) ==
  {m \in queue : ~(m.from = from /\ m.to \in (aliveSet \ {from}))} \cup
  {[from |-> from,
    to |-> to,
    core |-> state.core,
    committed |-> state.committed] :
      to \in (aliveSet \ {from})}

RECURSIVE SeqLess(_, _)

SeqLess(left, right) ==
  IF Len(left) = 0
  THEN Len(right) # 0
  ELSE IF Len(right) = 0
       THEN FALSE
       ELSE IF Head(left) < Head(right)
            THEN TRUE
            ELSE IF Head(left) > Head(right)
                 THEN FALSE
                 ELSE SeqLess(Tail(left), Tail(right))

RECURSIVE RemoveCommitted(_, _, _)

RemoveCommitted(seq, committedIds, i) ==
  IF i > Len(seq)
  THEN <<>>
  ELSE IF seq[i] \in committedIds
       THEN RemoveCommitted(seq, committedIds, i + 1)
       ELSE <<seq[i]>> \o RemoveCommitted(seq, committedIds, i + 1)

WithoutCommitted(seq, committed) ==
  RemoveCommitted(seq, SeqElems(committed), 1)

MergeCommitted(left, right) ==
  IF IsPrefix(left, right)
  THEN right
  ELSE IF IsPrefix(right, left)
       THEN left
       ELSE left

MergeNodesMessages(current, incoming) ==
  [n \in Nodes |->
     IF incoming[n].generation > current[n].generation \/
        /\ incoming[n].generation = current[n].generation
        /\ SeqLess(current[n].messages, incoming[n].messages)
     THEN incoming[n]
     ELSE current[n]]

MatchingPromises(promises, prefix) ==
  {p \in promises : p.prefix = prefix}

PromiseVotesFor(promises, prefix) ==
  UNION {p.votes : p \in MatchingPromises(promises, prefix)}

PutPromiseVotes(promises, prefix, votes) ==
  LET retained == {p \in promises : p.prefix # prefix}
  IN IF prefix = <<>> \/ votes = {}
     THEN retained
     ELSE retained \cup {[prefix |-> prefix, votes |-> votes]}

PrefixSupport(nodesMessages, aliveSet, prefix) ==
  {n \in aliveSet :
     /\ Len(nodesMessages[n].messages) >= Len(prefix)
     /\ prefix = SubSeq(nodesMessages[n].messages, 1, Len(prefix))}

NormalizePromises(promises, nodesMessages, aliveSet, proposals) ==
  {p \in PromiseState :
     LET support == PrefixSupport(nodesMessages, aliveSet, p.prefix)
         votes == PromiseVotesFor(promises, p.prefix) \cap support
     IN /\ p.prefix # <<>>
        /\ p.prefix \in MessageSeqs
        /\ SeqElems(p.prefix) \subseteq proposals
        /\ votes # {}
        /\ p.votes = votes}

RebaseGeneration(state, fromCommitted, toCommitted) ==
  LET delta == SuffixAfter(toCommitted, fromCommitted)
      messages0 == state.messages
      messages1 ==
        IF /\ delta # <<>>
           /\ IsPrefix(delta, messages0)
        THEN SuffixAfter(messages0, delta)
        ELSE messages0
  IN [state EXCEPT !.messages = WithoutCommitted(messages1, toCommitted)]

RebasePromises(promises, fromCommitted, toCommitted) ==
  LET delta == SuffixAfter(toCommitted, fromCommitted)
  IN UNION {
       LET prefix0 == promise.prefix
           prefix1 ==
             IF /\ delta # <<>>
                /\ IsPrefix(delta, prefix0)
             THEN SuffixAfter(prefix0, delta)
             ELSE prefix0
           prefix2 == WithoutCommitted(prefix1, toCommitted)
       IN IF prefix2 = <<>>
          THEN {}
          ELSE {[promise EXCEPT !.prefix = prefix2]}
       : promise \in promises}

RebaseCore(core, aliveSet, fromCommitted, toCommitted) ==
  LET nodes1 ==
        [n \in Nodes |->
          RebaseGeneration(core.nodesMessages[n], fromCommitted, toCommitted)]
      proposals1 == core.proposals \ SeqElems(toCommitted)
      promises1 ==
        NormalizePromises(
          RebasePromises(core.promises, fromCommitted, toCommitted),
          nodes1,
          aliveSet,
          proposals1)
  IN [proposals |-> proposals1,
      nodesMessages |-> nodes1,
      promises |-> promises1]

BumpGeneration(generation, steps, proposalCount) ==
  LET cap == 2 * proposalCount + 1
  IN IF generation + steps < cap
     THEN generation + steps
     ELSE cap

MajorityIds(nodesMessages, aliveSet, idx) ==
  {id \in MessageIds :
     Cardinality({n \in aliveSet :
                    /\ Len(nodesMessages[n].messages) >= idx
                    /\ nodesMessages[n].messages[idx] = id}) >=
       MajorityOf(aliveSet)}

RECURSIVE Iterate(_, _, _, _, _, _, _, _)

Iterate(core, self, idx, sorted, prefix, committed, aliveSet, proposalCount) ==
  IF idx > Cardinality(core.proposals)
  THEN [core |-> core, committed |-> committed]
  ELSE
    LET ids == MajorityIds(core.nodesMessages, aliveSet, idx)
    IN
      IF ids # {}
      THEN
        LET id == CHOOSE item \in ids : TRUE
            prefix1 == Append(prefix, id)
            support1 == PrefixSupport(core.nodesMessages, aliveSet, prefix1)
            votes0 ==
              PromiseVotesFor(core.promises, prefix1) \cup
              (IF self \in support1 THEN {self} ELSE {})
            votes1 == votes0 \cap support1
            committed1 ==
              IF /\ Cardinality(support1) >= MajorityOf(aliveSet)
                 /\ Cardinality(votes1) >= MajorityOf(aliveSet)
              THEN prefix1
              ELSE committed
            promises1 == PutPromiseVotes(core.promises, prefix1, votes1)
            core1 ==
              [core EXCEPT
                 !.promises =
                   NormalizePromises(
                     promises1,
                     core.nodesMessages,
                     aliveSet,
                     core.proposals)]
        IN Iterate(
             core1,
             self,
             idx + 1,
             sorted,
             prefix1,
             committed1,
             aliveSet,
             proposalCount)
      ELSE IF ~sorted
           THEN
             LET sortedMsgs == SortFrom(core.nodesMessages[self].messages, idx)
                 nodes1 ==
                   [core.nodesMessages EXCEPT ![self].messages = sortedMsgs]
                 core1 ==
                   IF sortedMsgs = core.nodesMessages[self].messages
                   THEN core
                   ELSE [core EXCEPT
                           !.nodesMessages[self].messages = sortedMsgs,
                           !.nodesMessages[self].generation =
                             BumpGeneration(@, 1, proposalCount),
                           !.promises =
                             NormalizePromises(@, nodes1, aliveSet, core.proposals)]
             IN Iterate(
                  core1,
                  self,
                  idx,
                  TRUE,
                  prefix,
                  committed,
                  aliveSet,
                  proposalCount)
           ELSE [core |-> core, committed |-> committed]

MergeResult(state, self, incoming, aliveSet, proposalCount) ==
  LET committed == MergeCommitted(state.committed, incoming.committed)
      currentCore ==
        RebaseCore(state.core, aliveSet, state.committed, committed)
      incomingCore ==
        RebaseCore(incoming.core, aliveSet, incoming.committed, committed)
      mergedNodes ==
        MergeNodesMessages(currentCore.nodesMessages, incomingCore.nodesMessages)
      newIds == NewProposalSeq(currentCore.proposals, incomingCore.proposals)
      self0 == mergedNodes[self]
      self1 ==
        IF Len(newIds) = 0
        THEN self0
        ELSE [self0 EXCEPT
                !.messages = @ \o newIds,
                !.generation = BumpGeneration(@, Len(newIds), proposalCount)]
      proposals1 == currentCore.proposals \cup incomingCore.proposals
      nodes1 == [mergedNodes EXCEPT ![self] = self1]
      promises1 ==
        NormalizePromises(
          currentCore.promises \cup incomingCore.promises,
          nodes1,
          aliveSet,
          proposals1)
      base ==
        [proposals |-> proposals1,
         nodesMessages |-> nodes1,
         promises |-> promises1]
      iter == Iterate(base, self, 1, FALSE, <<>>, <<>>, aliveSet, proposalCount)
      finalCommitted == committed \o iter.committed
      finalCore == RebaseCore(iter.core, aliveSet, committed, finalCommitted)
  IN [changed |-> (finalCore # state.core \/ finalCommitted # state.committed),
      core |-> finalCore,
      committed |-> finalCommitted]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ local[node] = EmptyNodeState
  /\ LET incoming ==
           [core |-> [EmptyCore EXCEPT !.proposals = {msg}],
            committed |-> <<>>]
         out ==
           MergeResult(
             local[node],
             node,
             incoming,
             alive,
             Cardinality(proposed \cup {msg}))
         state1 == [core |-> out.core, committed |-> out.committed]
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' = [local EXCEPT ![node] = state1]
       /\ stateMsgs' = BroadcastState(stateMsgs, node, state1, alive)

DeliverState(msg) ==
  /\ msg \in stateMsgs
  /\ msg.to \in alive
  /\ Comparable(local[msg.to].committed, msg.committed)
  /\ LET incoming ==
           [core |-> msg.core, committed |-> msg.committed]
         out ==
           MergeResult(
             local[msg.to],
             msg.to,
             incoming,
             alive,
             Cardinality(proposed))
         state1 == [core |-> out.core, committed |-> out.committed]
     IN
       /\ alive' = alive
       /\ proposed' = proposed
       /\ IF out.changed
          THEN /\ local' = [local EXCEPT ![msg.to] = state1]
               /\ stateMsgs' =
                    BroadcastState(stateMsgs \ {msg}, msg.to, state1, alive)
          ELSE /\ local' = local
               /\ stateMsgs' = stateMsgs \ {msg}

ProposeAny ==
  \E node \in Nodes : \E msg \in MessageIds : Propose(node, msg)

DeliverAnyState ==
  \E msg \in stateMsgs : DeliverState(msg)

Stabilize(node) ==
  /\ node \in alive
  /\ LET incoming ==
           [core |-> EmptyCore, committed |-> local[node].committed]
         out ==
           MergeResult(
             local[node],
             node,
             incoming,
             alive,
             Cardinality(proposed))
         state1 == [core |-> out.core, committed |-> out.committed]
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' = [local EXCEPT ![node] = state1]
       /\ stateMsgs' = BroadcastState(stateMsgs, node, state1, alive)

StabilizeAny ==
  \E node \in Nodes : Stabilize(node)

PruneFailedCore(core, aliveSet, failed) ==
  LET nodes1 ==
        [core.nodesMessages EXCEPT
          ![failed] = [messages |-> <<>>, generation |-> 0]]
  IN [core EXCEPT
        !.nodesMessages = nodes1,
        !.promises = NormalizePromises(core.promises, nodes1, aliveSet, core.proposals)]

DisconnectNodeState(node, failed) ==
  LET survivors == alive \ {failed}
      current == local[node]
      next ==
        [current EXCEPT
          !.core = PruneFailedCore(current.core, survivors, failed)]
      out ==
        MergeResult(
          next,
          node,
          [core |-> EmptyCore, committed |-> next.committed],
          survivors,
          Cardinality(proposed))
  IN [core |-> out.core, committed |-> out.committed]

DisconnectChangedNodes(failed) ==
  {node \in (alive \ {failed}) :
     DisconnectNodeState(node, failed) # local[node]}

DisconnectStateMessages(failed) ==
  LET survivors == alive \ {failed}
      changed == DisconnectChangedNodes(failed)
      retainedMsgs ==
        {msg \in stateMsgs :
           /\ msg.from # failed
           /\ msg.to # failed
           /\ msg.from \notin changed}
      retained ==
        {[msg EXCEPT !.core = PruneFailedCore(msg.core, survivors, failed)] :
           msg \in retainedMsgs}
      rebroadcast ==
        UNION {{[from |-> from,
                  to |-> to,
                  core |-> DisconnectNodeState(from, failed).core,
                  committed |-> DisconnectNodeState(from, failed).committed] :
                   to \in (survivors \ {from})} :
                from \in changed}
  IN retained \cup rebroadcast

Disconnect(failed) ==
  /\ failed \in alive
  /\ alive' = alive \ {failed}
  /\ proposed' = proposed
  /\ local' =
       [node \in Nodes |->
         IF node \in (alive \ {failed})
         THEN DisconnectNodeState(node, failed)
         ELSE local[node]]
  /\ stateMsgs' = DisconnectStateMessages(failed)

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
  \/ DeliverAnyState
  \/ StabilizeAny

LiveNext ==
  \/ ProposeAny
  \/ DeliverAnyState
  \/ StabilizeAny
  \/ LiveDisconnectAny

CoreWellFormed(core, aliveSet, committed) ==
  /\ core \in CoreState
  /\ core.proposals \subseteq proposed
  /\ core.proposals \cap SeqElems(committed) = {}
  /\ \A n \in Nodes :
       /\ SeqElems(core.nodesMessages[n].messages) \subseteq core.proposals
       /\ NoDuplicates(core.nodesMessages[n].messages)
  /\ \A promise \in core.promises :
       /\ promise.prefix # <<>>
       /\ promise.prefix \in MessageSeqs
       /\ SeqElems(promise.prefix) \subseteq core.proposals
       /\ NoDuplicates(promise.prefix)
       /\ promise.votes \subseteq PrefixSupport(core.nodesMessages, aliveSet, promise.prefix)

TypeOK ==
  /\ alive \subseteq Nodes
  /\ proposed \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ stateMsgs \subseteq StateMessage

LocalWellFormed ==
  \A n \in alive :
    /\ CoreWellFormed(local[n].core, alive, local[n].committed)
    /\ local[n].committed \in MessageSeqs
    /\ SeqElems(local[n].committed) \subseteq proposed
    /\ NoDuplicates(local[n].committed)

MessageWellFormed ==
  \A msg \in stateMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ CoreWellFormed(msg.core, alive, msg.committed)
    /\ msg.committed \in MessageSeqs
    /\ SeqElems(msg.committed) \subseteq proposed
    /\ NoDuplicates(msg.committed)

PrefixAgreement ==
  \A left \in alive :
    \A right \in alive :
      Comparable(local[left].committed, local[right].committed)

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ MessageWellFormed
  /\ PrefixAgreement

CommitHappened ==
  /\ alive # {}
  /\ \A node \in alive : local[node].committed # <<>>

Termination == <>CommitHappened

Spec == Init /\ [][Next]_vars

LiveSpec ==
  /\ Init
  /\ [][LiveNext]_vars
  /\ WF_vars(ProposeAny)
  /\ WF_vars(DeliverAnyState)
  /\ WF_vars(StabilizeAny)
  /\ Termination

=============================================================================
