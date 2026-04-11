#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "../tests/engine_fixture.h"
#include "algorithm.h"
#include "boolean.h"
#include "error.h"
#include "extractor.h"
#include "flat.h"
#include "field.h"
#include "infix.h"
#include "operation.h"
#include "quantifier.h"

namespace leaderless_consensus {

using test::EngineFixture;

// Reference:
// https://gridem.blogspot.com/2016/05/replicated-object-part-7-masterless.html

using NodeId = int;
using MessageId = int;
using NodeSet = FlatSet<NodeId>;
using ProposalSet = FlatSet<MessageId>;
using MessageSeq = std::vector<MessageId>;

inline const ProposalSet kProposalIds = {10, 11, 12};

template <typename T_set>
T_set setUnion(T_set left, const T_set& right) {
  left.insert(right.begin(), right.end());
  return left;
}

template <typename T_set>
T_set setIntersection(const T_set& left, const T_set& right) {
  T_set result;
  for (auto&& item : left) {
    if (right.contains(item)) {
      result.insert(item);
    }
  }
  return result;
}

template <typename T_set, typename T_value>
T_set setWithout(T_set set, const T_value& value) {
  set.erase(value);
  return set;
}

template <typename T_left, typename T_right>
bool isSubset(const T_left& left, const T_right& right) {
  for (auto&& item : left) {
    if (!right.contains(item)) {
      return false;
    }
  }
  return true;
}

template <typename T_map, typename T_key>
typename T_map::mapped_type findOrEmpty(const T_map& map, const T_key& key) {
  auto it = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  return {};
}

template <typename T>
bool isPrefix(const std::vector<T>& prefix, const std::vector<T>& values) {
  if (prefix.size() > values.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (prefix[i] != values[i]) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool itemsAreSubset(const std::vector<T>& values, const ProposalSet& allowed) {
  for (auto&& value : values) {
    if (!allowed.contains(value)) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool allUnique(const std::vector<T>& values) {
  FlatSet<T> seen;
  for (auto&& value : values) {
    if (!seen.insert(value).second) {
      return false;
    }
  }
  return true;
}

template <typename T_messages>
T_messages purgeMessages(const T_messages& messages, NodeId failed) {
  T_messages result;
  for (auto&& message : messages) {
    if (message.from != failed && message.to != failed) {
      result.insert(message);
    }
  }
  return result;
}

template <typename T_messages>
bool queueEndpointsAreAlive(const T_messages& messages, const NodeSet& alive) {
  for (auto&& message : messages) {
    if (!alive.contains(message.from) || !alive.contains(message.to)) {
      return false;
    }
  }
  return true;
}

}  // namespace leaderless_consensus
