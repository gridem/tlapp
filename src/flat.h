#pragma once

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T, typename Compare = std::less<T>>
struct FlatSet {
  using value_type = T;
  using storage_type = std::vector<T>;
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  FlatSet() = default;

  FlatSet(std::initializer_list<T> items) {
    insert(items.begin(), items.end());
  }

  template <typename T_iter>
  FlatSet(T_iter first, T_iter last) {
    insert(first, last);
  }

  bool operator==(const FlatSet& other) const {
    return items_ == other.items_;
  }

  bool operator<(const FlatSet& other) const {
    return items_ < other.items_;
  }

  bool empty() const {
    return items_.empty();
  }

  size_t size() const {
    return items_.size();
  }

  iterator begin() {
    return items_.begin();
  }

  iterator end() {
    return items_.end();
  }

  const_iterator begin() const {
    return items_.begin();
  }

  const_iterator end() const {
    return items_.end();
  }

  const T& at(size_t index) const {
    return items_.at(index);
  }

  bool contains(const T& value) const {
    return find(value) != end();
  }

  std::pair<iterator, bool> insert(const T& value) {
    auto it = lowerBound(value);
    if (it != end() && !compare(value, *it) && !compare(*it, value)) {
      return {it, false};
    }
    it = items_.insert(it, value);
    return {it, true};
  }

  std::pair<iterator, bool> insert(T&& value) {
    auto it = lowerBound(value);
    if (it != end() && !compare(value, *it) && !compare(*it, value)) {
      return {it, false};
    }
    it = items_.insert(it, std::move(value));
    return {it, true};
  }

  iterator insert(const_iterator, const T& value) {
    return insert(value).first;
  }

  iterator insert(const_iterator, T&& value) {
    return insert(std::move(value)).first;
  }

  template <typename T_iter>
  void insert(T_iter first, T_iter last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  size_t erase(const T& value) {
    auto it = find(value);
    if (it == end()) {
      return 0;
    }
    items_.erase(it);
    return 1;
  }

  iterator erase(const_iterator it) {
    return items_.erase(it);
  }

  void clear() {
    items_.clear();
  }

  iterator find(const T& value) {
    auto it = lowerBound(value);
    if (it != end() && !compare(value, *it) && !compare(*it, value)) {
      return it;
    }
    return end();
  }

  const_iterator find(const T& value) const {
    auto it = lowerBound(value);
    if (it != end() && !compare(value, *it) && !compare(*it, value)) {
      return it;
    }
    return end();
  }

 private:
  static bool compare(const T& left, const T& right) {
    return Compare{}(left, right);
  }

  iterator lowerBound(const T& value) {
    return std::lower_bound(items_.begin(), items_.end(), value, compare);
  }

  const_iterator lowerBound(const T& value) const {
    return std::lower_bound(items_.begin(), items_.end(), value, compare);
  }

  storage_type items_;
};

template <typename K, typename V, typename Compare = std::less<K>>
struct FlatMap {
  using key_type = K;
  using mapped_type = V;
  using value_type = std::pair<K, V>;
  using storage_type = std::vector<value_type>;
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  FlatMap() = default;

  FlatMap(std::initializer_list<value_type> items) {
    insert(items.begin(), items.end());
  }

  bool operator==(const FlatMap& other) const {
    return items_ == other.items_;
  }

  bool operator<(const FlatMap& other) const {
    return items_ < other.items_;
  }

  bool empty() const {
    return items_.empty();
  }

  size_t size() const {
    return items_.size();
  }

  iterator begin() {
    return items_.begin();
  }

  iterator end() {
    return items_.end();
  }

  const_iterator begin() const {
    return items_.begin();
  }

  const_iterator end() const {
    return items_.end();
  }

  bool contains(const K& key) const {
    return find(key) != end();
  }

  V& operator[](const K& key) {
    auto it = lowerBound(key);
    if (it == end() || compare(key, it->first) || compare(it->first, key)) {
      it = items_.insert(it, value_type{key, {}});
    }
    return it->second;
  }

  V& at(const K& key) {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("FlatMap::at");
    }
    return it->second;
  }

  const V& at(const K& key) const {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("FlatMap::at");
    }
    return it->second;
  }

  std::pair<iterator, bool> insert(const value_type& value) {
    auto it = lowerBound(value.first);
    if (it != end() &&
        !compare(value.first, it->first) &&
        !compare(it->first, value.first)) {
      return {it, false};
    }
    it = items_.insert(it, value);
    return {it, true};
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    auto it = lowerBound(value.first);
    if (it != end() &&
        !compare(value.first, it->first) &&
        !compare(it->first, value.first)) {
      return {it, false};
    }
    it = items_.insert(it, std::move(value));
    return {it, true};
  }

  template <typename T_iter>
  void insert(T_iter first, T_iter last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  iterator find(const K& key) {
    auto it = lowerBound(key);
    if (it != end() && !compare(key, it->first) && !compare(it->first, key)) {
      return it;
    }
    return end();
  }

  const_iterator find(const K& key) const {
    auto it = lowerBound(key);
    if (it != end() && !compare(key, it->first) && !compare(it->first, key)) {
      return it;
    }
    return end();
  }

  size_t erase(const K& key) {
    auto it = find(key);
    if (it == end()) {
      return 0;
    }
    items_.erase(it);
    return 1;
  }

  iterator erase(const_iterator it) {
    return items_.erase(it);
  }

  void clear() {
    items_.clear();
  }

 private:
  static bool compare(const K& left, const K& right) {
    return Compare{}(left, right);
  }

  iterator lowerBound(const K& key) {
    return std::lower_bound(items_.begin(), items_.end(), key,
        [](const auto& item, const K& value) { return compare(item.first, value); });
  }

  const_iterator lowerBound(const K& key) const {
    return std::lower_bound(items_.begin(), items_.end(), key,
        [](const auto& item, const K& value) { return compare(item.first, value); });
  }

  storage_type items_;
};
