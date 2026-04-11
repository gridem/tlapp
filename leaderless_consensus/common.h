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
using NodeSet = std::set<NodeId>;
using ProposalSet = std::set<MessageId>;
using MessageSeq = std::vector<MessageId>;

template <typename T>
std::set<T> setUnion(std::set<T> left, const std::set<T>& right) {
  left.insert(right.begin(), right.end());
  return left;
}

template <typename T>
std::set<T> setIntersection(const std::set<T>& left, const std::set<T>& right) {
  std::set<T> result;
  for (auto&& item : left) {
    if (right.contains(item)) {
      result.insert(item);
    }
  }
  return result;
}

template <typename T>
std::set<T> setWithout(std::set<T> set, const T& value) {
  set.erase(value);
  return set;
}

template <typename T>
bool isSubset(const std::set<T>& left, const std::set<T>& right) {
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
bool itemsAreSubset(const std::vector<T>& values, const std::set<T>& allowed) {
  for (auto&& value : values) {
    if (!allowed.contains(value)) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool allUnique(const std::vector<T>& values) {
  std::set<T> seen;
  for (auto&& value : values) {
    if (!seen.insert(value).second) {
      return false;
    }
  }
  return true;
}

template <typename T_message>
std::set<T_message> purgeMessages(const std::set<T_message>& messages, NodeId failed) {
  std::set<T_message> result;
  for (auto&& message : messages) {
    if (message.from != failed && message.to != failed) {
      result.insert(message);
    }
  }
  return result;
}

template <typename T_message>
bool queueEndpointsAreAlive(const std::set<T_message>& messages, const NodeSet& alive) {
  for (auto&& message : messages) {
    if (!alive.contains(message.from) || !alive.contains(message.to)) {
      return false;
    }
  }
  return true;
}

}  // namespace leaderless_consensus
