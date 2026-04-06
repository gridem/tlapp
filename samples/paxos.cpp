#include <gtest/gtest.h>

#include <map>

#include "../tests/engine_fixture.h"
#include "as_string.h"
#include "boolean.h"
#include "extractor.h"
#include "field.h"
#include "functional.h"
#include "infix.h"
#include "operation.h"
#include "quantifier.h"

namespace paxos {

using test::EngineFixture;

enum MessageType {
  M1a = 0,
  M1b,
  M2a,
  M2b,
};

struct Message : hashable_tag_type {
  explicit Message(MessageType type_, int acc_ = -1, int bal_ = -1,
                   int val_ = -1, int mbal_ = -1, int mval_ = -1)
      : type(type_),
        acc(acc_),
        bal(bal_),
        val(val_),
        mbal(mbal_),
        mval(mval_) {}

  int type;
  int acc;
  int bal;
  int val;
  int mbal;
  int mval;

  fields(type, acc, bal, val, mbal, mval);
};

using Set = std::set<int>;
using Map = std::map<int, int>;

using Ballots = Set;
using Acceptors = Set;
using Values = Set;
using Quorum = Set;
using Quorums = std::set<Quorum>;

using Messages = std::set<Message>;

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/PaxosHowToWinATuringAward/Paxos.tla
struct Model : IModel {
  fun(send, m) {
    return msgs++ == (msgs $cup m);
  }

  funs(sendMessage, args) { return send(creator<Message>(fwd(args)...)); }

  fun(send1a, bal) { return sendMessage(M1a, -1, fwd(bal)); }

  fun(send1b, acc, bal, mbal, mval) {
    return sendMessage(M1b, fwd(acc, bal), -1, fwd(mbal, mval));
  }

  fun(send2a, bal, val) { return sendMessage(M2a, -1, fwd(bal, val)); }

  fun(send2b, acc, bal, val) { return sendMessage(M2b, fwd(acc, bal, val)); }

  fun(showsSafeAt, b, v) {
    return $E(q, quorum) {
      return $A(a, q) {
        return $E(m, msgs) {
          return get_mem(m, type) == M1b && get_mem(m, bal) == b &&
                 get_mem(m, acc) == a;
        };
      } && $A(m1, msgs) {
        return !(get_mem(m1, type) == M1b && get_mem(m1, bal) == b &&
                 get_mem(m1, acc) $in q && get_mem(m1, mbal) >= 0) ||
               $A(m2, msgs) {
                 return !(get_mem(m2, type) == M1b && get_mem(m2, bal) == b &&
                          get_mem(m2, acc) $in q && get_mem(m2, mbal) >= 0) ||
                        get_mem(m1, mbal) == get_mem(m2, mbal);
               };
      } &&
             ($A(m, msgs) {
               return !(get_mem(m, type) == M1b && get_mem(m, bal) == b &&
                        get_mem(m, acc) $in q && get_mem(m, mbal) >= 0);
             } ||
              $E(m, msgs) {
                return get_mem(m, type) == M1b && get_mem(m, bal) == b &&
                       get_mem(m, acc) $in q && get_mem(m, mbal) >= 0 &&
                       get_mem(m, mval) == v;
              });
    };
  }

  fun(phase1a, b) {
    return !($E(m, msgs) {
      return get_mem(m, type) == M1a && get_mem(m, bal) == b;
    }) && send1a(fwd(b)) &&
           unchanged(maxBal, maxVBal, maxVal);
  }

  fun(phase1b, a) {
    return $E(m, msgs) {
      return get_mem(m, type) == M1a && get_mem(m, bal) > at(maxBal, a) &&
             mutAt(maxBal, a, get_mem(m, bal)) &&
             send1b(a, get_mem(m, bal), at(maxVBal, a), at(maxVal, a));
    }
    &&unchanged(maxVBal, maxVal);
  }

  fun(phase2a, b, v) {
    return !($E(m, msgs) {
             return get_mem(m, type) == M2a && get_mem(m, bal) == b;
           }) &&
           showsSafeAt(b, v) && send2a(b, v) &&
           unchanged(maxVal, maxVBal, maxBal);
  }

  fun(phase2b, a) {
    return $E(m, msgs) {
      return get_mem(m, type) == M2a && get_mem(m, bal) >= at(maxBal, a) &&
             mutAt(maxBal, a, get_mem(m, bal)) &&
             mutAt(maxVBal, a, get_mem(m, bal)) &&
             mutAt(maxVal, a, get_mem(m, val)) &&
             send2b(a, get_mem(m, bal), get_mem(m, val));
    };
  }

  Map createMap(int v) {
    Map result;
    for (auto&& a : acceptor) {
      result[a] = v;
    }
    return result;
  }

  Boolean init() override {
    return maxBal == createMap(-1) && maxVBal == createMap(-1) &&
           maxVal == createMap(-1) && msgs == Messages{};
  }

  Boolean next() override {
    return $E(b, ballot) {
      return phase1a(b) || $E(v, value) { return phase2a(b, v); };
    }
    || $E(a, acceptor) { return phase1b(a) || phase2b(a); };
  }

  std::optional<Boolean> ensure() override {
    return $A(a, acceptor) {
      return at(maxBal, a) >= at(maxVBal, a);
    } /*&& $A(a, acceptor) {}*/ /*&& $A(m, msgs) {
return get_mem(m, type) == M1b;
}*/;
  }
  /*
Inv ==
 /\ TypeOK
 /\ \A a \in Acceptor : maxBal[a] >= maxVBal[a]
 /\ \A a \in Acceptor : IF maxVBal[a] = -1
                          THEN maxVal[a] = None
                          ELSE <<maxVBal[a], maxVal[a]>> \in votes[a]
 /\ \A m \in msgs :
       /\ (m.type = "1b") => /\ maxBal[m.acc] >= m.bal
                             /\ (m.mbal >= 0) =>
                                 <<m.mbal, m.mval>> \in votes[m.acc]
       /\ (m.type = "2a") => /\ \E Q \in Quorum :
                                   V!ShowsSafeAt(Q, m.bal, m.val)
                             /\ \A mm \in msgs : /\ mm.type ="2a"
                                                 /\ mm.bal = m.bal
                                                 => mm.val = m.val
       /\ (m.type = "2b") => /\ maxVBal[m.acc] >= m.bal
                             /\ \E mm \in msgs : /\ mm.type = "2a"
                                                 /\ mm.bal  = m.bal
                                                 /\ mm.val  = m.val
  */

  Var<Map> maxBal{"maxBal"};
  Var<Map> maxVBal{"maxVBal"};
  Var<Map> maxVal{"maxVal"};
  Var<Messages> msgs{"msgs"};

  Ballots ballot = {1, 2, 3};
  Acceptors acceptor = {1, 2, 3};
  Values value = {1, 2};
  Quorums quorum = {{1, 2}, {1, 3}, {2, 3}};
};

TEST_F(EngineFixture, Paxos) {
  e.createModel<Model>();
  e.run();
}

}  // namespace paxos
