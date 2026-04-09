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

PromiseSeqs == AllMessageSeqs

MaxGeneration == 4

Majority == Cardinality(Nodes) \div 2 + 1

GenerationState ==
  [messages : MessageSeqs, generation : 0..MaxGeneration]

PromiseKey ==
  [prefix : PromiseSeqs, support : SUBSET Nodes]

PromiseMap ==
  [PromiseKey -> SUBSET Nodes]

CoreState ==
  [carries : SUBSET MessageIds,
   nodesMessages : [Nodes -> GenerationState],
   promises : PromiseMap]

NodeState ==
  [core : CoreState, committed : MessageSeqs]

StateMessage ==
  [from : Nodes, to : Nodes, core : CoreState]

VARIABLES alive, applied, local, stateMsgs

vars == <<alive, applied, local, stateMsgs>>

InitPromises == [key \in PromiseKey |-> {}]

EmptyNodesMessages ==
  [n \in Nodes |-> [messages |-> <<>>, generation |-> 0]]

InitLocal ==
  [n \in Nodes |->
    [core |-> [carries |-> {},
               nodesMessages |-> EmptyNodesMessages,
               promises |-> InitPromises],
     committed |-> <<>>]]

Init ==
  /\ alive = Nodes
  /\ applied = {}
  /\ local = InitLocal
  /\ stateMsgs = {}

SeqElems(seq) == {seq[i] : i \in DOMAIN seq}

IsPrefix(left, right) ==
  /\ Len(left) <= Len(right)
  /\ left = SubSeq(right, 1, Len(left))

Comparable(left, right) ==
  IsPrefix(left, right) \/ IsPrefix(right, left)

MinSet(set) ==
  CHOOSE item \in set : \A other \in set : item <= other

SortedSeqFromSet(set) ==
  IF set = {}
  THEN <<>>
  ELSE IF Cardinality(set) = 1
       THEN <<CHOOSE item \in set : TRUE>>
       ELSE <<MinSet(set), CHOOSE item \in set : item # MinSet(set)>>

NewCarrySeq(oldCarries, incomingCarries) ==
  SortedSeqFromSet(incomingCarries \ oldCarries)

SortFrom(seq, idx) ==
  IF idx >= Len(seq)
  THEN seq
  ELSE IF seq[idx] <= seq[idx + 1]
       THEN seq
       ELSE [i \in DOMAIN seq |->
               IF i = idx
               THEN seq[idx + 1]
               ELSE IF i = idx + 1
                    THEN seq[idx]
                    ELSE seq[i]]

BroadcastState(queue, from, core, aliveSet) ==
  queue \cup {[from |-> from, to |-> to, core |-> core] :
                to \in (aliveSet \ {from})}

MergeNodesMessages(current, incoming) ==
  [n \in Nodes |->
     IF incoming[n].generation > current[n].generation
     THEN incoming[n]
     ELSE current[n]]

MergePromises(left, right) ==
  [key \in PromiseKey |-> left[key] \cup right[key]]

BaseCore(core, self, incoming) ==
  LET mergedNodes == MergeNodesMessages(core.nodesMessages, incoming.nodesMessages)
      newIds == NewCarrySeq(core.carries, incoming.carries)
      self0 == mergedNodes[self]
      self1 ==
        IF Len(newIds) = 0
        THEN self0
        ELSE [self0 EXCEPT
                !.messages = @ \o newIds,
                !.generation =
                  IF @ < MaxGeneration THEN @ + 1 ELSE MaxGeneration]
  IN
    [carries |-> core.carries \cup incoming.carries,
     nodesMessages |-> [mergedNodes EXCEPT ![self] = self1],
     promises |-> MergePromises(core.promises, incoming.promises)]

MajorityIds(nodesMessages, idx) ==
  {id \in MessageIds :
     Cardinality({n \in Nodes :
                    /\ Len(nodesMessages[n].messages) >= idx
                    /\ nodesMessages[n].messages[idx] = id}) >= Majority}

PrefixSupport(nodesMessages, prefix) ==
  {n \in Nodes :
     /\ Len(nodesMessages[n].messages) >= Len(prefix)
     /\ prefix = SubSeq(nodesMessages[n].messages, 1, Len(prefix))}

RECURSIVE Iterate(_, _, _, _, _, _, _, _)

Iterate(core, oldPromises, incomingPromises, self, idx, sorted, prefix, committed) ==
  IF idx > Cardinality(core.carries)
  THEN [core |-> core, committed |-> committed]
  ELSE
    LET ids == MajorityIds(core.nodesMessages, idx)
    IN
      IF ids # {}
      THEN
        LET id == CHOOSE item \in ids : TRUE
            prefix1 == Append(prefix, id)
            support1 == PrefixSupport(core.nodesMessages, prefix1)
            key1 == [prefix |-> prefix1, support |-> support1]
            votes0 ==
              core.promises[key1] \cup
              (IF self \in support1 THEN {self} ELSE {})
            votes == votes0 \cap support1
            core1 == [core EXCEPT !.promises[key1] = votes]
            committed1 ==
              IF /\ Cardinality(support1) >= Majority
                 /\ Cardinality(votes) >= Majority
              THEN prefix1
              ELSE committed
        IN Iterate(core1, oldPromises, incomingPromises, self, idx + 1, sorted,
                   prefix1, committed1)
      ELSE IF ~sorted
           THEN
             LET sortedMsgs == SortFrom(core.nodesMessages[self].messages, idx)
                 core1 ==
                   IF sortedMsgs = core.nodesMessages[self].messages
                   THEN core
                   ELSE [core EXCEPT
                           !.nodesMessages[self].messages = sortedMsgs,
                           !.nodesMessages[self].generation =
                             IF @ < MaxGeneration THEN @ + 1 ELSE MaxGeneration]
             IN Iterate(core1, oldPromises, incomingPromises, self, idx, TRUE,
                        prefix, committed)
           ELSE [core |-> core, committed |-> committed]

MergeResult(state, self, incoming) ==
  LET base == BaseCore(state.core, self, incoming)
      iter ==
        Iterate(base, state.core.promises, incoming.promises, self,
                Len(state.committed) + 1, FALSE, state.committed,
                state.committed)
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

Apply(node, msg) ==
  /\ node \in alive
  /\ msg \notin applied
  /\ local[node] = InitLocal[node]
  /\ LET incoming ==
           [carries |-> {msg},
            nodesMessages |-> EmptyNodesMessages,
            promises |-> InitPromises]
         out == MergeResult(local[node], node, incoming)
     IN
       /\ out.changed
       /\ alive' = alive
       /\ applied' = applied \cup {msg}
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
       /\ applied' = applied
       /\ local' = [local EXCEPT ![msg.to] = [core |-> out.core,
                                              committed |-> out.committed]]
       /\ stateMsgs' = BroadcastState(stateMsgs \ {msg}, msg.to, out.core, alive)

Disconnect(failed) ==
  /\ failed \in alive
  /\ alive' = alive \ {failed}
  /\ applied' = applied
  /\ local' = local
  /\ stateMsgs' = {m \in stateMsgs : m.from # failed /\ m.to # failed}

Next ==
  \/ \E node \in Nodes : \E msg \in MessageIds : Apply(node, msg)
  \/ \E msg \in stateMsgs : DeliverState(msg)
  \/ \E failed \in Nodes : Disconnect(failed)

CoreWellFormed(core) ==
  /\ core \in CoreState
  /\ core.carries \subseteq applied
  /\ \A n \in Nodes :
       /\ SeqElems(core.nodesMessages[n].messages) \subseteq applied
       /\ NoDuplicates(core.nodesMessages[n].messages)
  /\ \A key \in PromiseKey :
       IF core.promises[key] = {}
       THEN TRUE
       ELSE /\ SeqElems(key.prefix) \subseteq applied
            /\ key.support \subseteq Nodes
            /\ core.promises[key] \subseteq key.support

TypeOK ==
  /\ alive \subseteq Nodes
  /\ applied \subseteq MessageIds
  /\ local \in [Nodes -> NodeState]
  /\ stateMsgs \subseteq StateMessage

LocalWellFormed ==
  \A n \in Nodes :
    /\ CoreWellFormed(local[n].core)
    /\ local[n].committed \in MessageSeqs
    /\ SeqElems(local[n].committed) \subseteq applied
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

Spec == Init /\ [][Next]_vars

=============================================================================
