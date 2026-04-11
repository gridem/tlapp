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
  [from : Nodes, to : Nodes, core : CoreState]

VARIABLES alive, proposed, local, stateMsgs

vars == <<alive, proposed, local, stateMsgs>>

InitPromises == {}

EmptyNodesMessages ==
  [n \in Nodes |-> [messages |-> <<>>, generation |-> 0]]

InitLocal ==
  [n \in Nodes |->
    [core |-> [proposals |-> {},
               nodesMessages |-> EmptyNodesMessages,
               promises |-> InitPromises],
     committed |-> <<>>]]

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

BroadcastState(queue, from, core, aliveSet) ==
  {m \in queue : ~(m.from = from /\ m.to \in (aliveSet \ {from}))} \cup
  {[from |-> from, to |-> to, core |-> core] :
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
  IN IF votes = {}
     THEN retained
     ELSE retained \cup {[prefix |-> prefix, votes |-> votes]}

NormalizePromises(promises, nodesMessages, proposals, committed) ==
  {p \in PromiseState :
     LET support == PrefixSupport(nodesMessages, p.prefix)
         votes == PromiseVotesFor(promises, p.prefix) \cap support
     IN /\ SeqElems(p.prefix) \subseteq proposals
        /\ ~IsPrefix(p.prefix, committed)
        /\ votes # {}
        /\ p.votes = votes}

BumpGeneration(generation, steps) ==
  IF generation + steps < MaxGeneration
  THEN generation + steps
  ELSE MaxGeneration

BaseCore(core, self, incoming, committed) ==
  LET mergedNodes == MergeNodesMessages(core.nodesMessages, incoming.nodesMessages)
      newIds == NewProposalSeq(core.proposals, incoming.proposals)
      self0 == mergedNodes[self]
      self1 ==
        IF Len(newIds) = 0
        THEN self0
        ELSE [self0 EXCEPT
                !.messages = @ \o newIds,
                !.generation = BumpGeneration(@, Len(newIds))]
      proposals1 == core.proposals \cup incoming.proposals
      nodes1 == [mergedNodes EXCEPT ![self] = self1]
  IN
    [proposals |-> proposals1,
     nodesMessages |-> nodes1,
     promises |-> NormalizePromises(core.promises \cup incoming.promises,
                                    nodes1, proposals1, committed)]

MajorityIds(nodesMessages, idx) ==
  {id \in MessageIds :
     Cardinality({n \in Nodes :
                    /\ Len(nodesMessages[n].messages) >= idx
                    /\ nodesMessages[n].messages[idx] = id}) >= Majority}

PrefixSupport(nodesMessages, prefix) ==
  {n \in Nodes :
     /\ Len(nodesMessages[n].messages) >= Len(prefix)
     /\ prefix = SubSeq(nodesMessages[n].messages, 1, Len(prefix))}

RECURSIVE Iterate(_, _, _, _, _, _)

Iterate(core, self, idx, sorted, prefix, committed) ==
  IF idx > Cardinality(core.proposals)
  THEN [core |-> core, committed |-> committed]
  ELSE
    LET ids == MajorityIds(core.nodesMessages, idx)
    IN
      IF ids # {}
      THEN
        LET id == CHOOSE item \in ids : TRUE
            prefix1 == Append(prefix, id)
            support1 == PrefixSupport(core.nodesMessages, prefix1)
            votes0 ==
              PromiseVotesFor(core.promises, prefix1) \cup
              (IF self \in support1 THEN {self} ELSE {})
            votes1 == votes0 \cap support1
            committed1 ==
              IF /\ Cardinality(support1) >= Majority
                 /\ Cardinality(votes1) >= Majority
              THEN prefix1
              ELSE committed
            promises1 == PutPromiseVotes(core.promises, prefix1, votes1)
            core1 ==
              [core EXCEPT
                 !.promises =
                   NormalizePromises(promises1, core.nodesMessages,
                                     core.proposals, committed1)]
        IN Iterate(core1, self, idx + 1, sorted, prefix1, committed1)
      ELSE IF ~sorted
           THEN
             LET sortedMsgs == SortFrom(core.nodesMessages[self].messages, idx)
                 core1 ==
                   IF sortedMsgs = core.nodesMessages[self].messages
                   THEN core
                   ELSE [core EXCEPT
                           !.nodesMessages[self].messages = sortedMsgs,
                           !.nodesMessages[self].generation =
                             BumpGeneration(@, 1),
                           !.promises =
                             NormalizePromises(@, [core.nodesMessages EXCEPT ![self].messages = sortedMsgs],
                                               core.proposals, committed)]
             IN Iterate(core1, self, idx, TRUE, prefix, committed)
           ELSE [core |-> core, committed |-> committed]

MergeResult(state, self, incoming) ==
  LET base == BaseCore(state.core, self, incoming, state.committed)
      iter ==
        Iterate(base, self, Len(state.committed) + 1, FALSE,
                state.committed, state.committed)
  IN
    IF iter.core = state.core
    THEN [changed |-> FALSE,
          core |-> state.core,
          committed |-> state.committed]
    ELSE [changed |-> TRUE,
          core |-> iter.core,
          committed |->
            IF Len(iter.committed) > Len(state.committed)
            THEN iter.committed
            ELSE state.committed]

Propose(node, msg) ==
  /\ node \in alive
  /\ msg \notin proposed
  /\ local[node] = InitLocal[node]
  /\ LET incoming ==
           [proposals |-> {msg},
            nodesMessages |-> EmptyNodesMessages,
            promises |-> InitPromises]
         out == MergeResult(local[node], node, incoming)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed \cup {msg}
       /\ local' = [local EXCEPT ![node] = [core |-> out.core,
                                            committed |-> out.committed]]
       /\ stateMsgs' = BroadcastState(stateMsgs, node, out.core, alive)

DeliverState(msg) ==
  /\ msg \in stateMsgs
  /\ msg.to \in alive
  /\ LET out == MergeResult(local[msg.to], msg.to, msg.core)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ proposed' = proposed
       /\ local' = [local EXCEPT ![msg.to] = [core |-> out.core,
                                              committed |-> out.committed]]
       /\ stateMsgs' = BroadcastState(stateMsgs \ {msg}, msg.to, out.core, alive)

Next ==
  \/ \E node \in Nodes : \E msg \in MessageIds : Propose(node, msg)
  \/ \E msg \in stateMsgs : DeliverState(msg)

CoreWellFormed(core) ==
  /\ core \in CoreState
  /\ core.proposals \subseteq proposed
  /\ \A n \in Nodes :
       /\ SeqElems(core.nodesMessages[n].messages) \subseteq proposed
       /\ NoDuplicates(core.nodesMessages[n].messages)
  /\ \A promise \in core.promises :
       /\ SeqElems(promise.prefix) \subseteq proposed
       /\ promise.votes \subseteq PrefixSupport(core.nodesMessages, promise.prefix)

TypeOK ==
  /\ alive \subseteq Nodes
  /\ proposed \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ stateMsgs \subseteq StateMessage

LocalWellFormed ==
  \A n \in Nodes :
    /\ CoreWellFormed(local[n].core)
    /\ local[n].committed \in MessageSeqs
    /\ SeqElems(local[n].committed) \subseteq proposed
    /\ NoDuplicates(local[n].committed)

MessageWellFormed ==
  \A msg \in stateMsgs :
    /\ msg.from \in alive
    /\ msg.to \in alive
    /\ CoreWellFormed(msg.core)

PrefixAgreement ==
  \A left \in Nodes :
    \A right \in Nodes :
      Comparable(local[left].committed, local[right].committed)

Invariant ==
  /\ TypeOK
  /\ LocalWellFormed
  /\ MessageWellFormed
  /\ PrefixAgreement

CanProposeAny ==
  \E node \in Nodes :
    \E msg \in MessageIds :
      /\ node \in alive
      /\ msg \notin proposed
      /\ local[node] = InitLocal[node]

Quiescent ==
  /\ stateMsgs = {}
  /\ ~CanProposeAny

Termination == <>Quiescent

Spec == Init /\ [][Next]_vars
LiveSpec == Spec /\ WF_vars(Next)

=============================================================================
