#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T, size_t InlineCapacity = 8>
struct InplaceVector {
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  InplaceVector() = default;

  InplaceVector(std::initializer_list<T> items) {
    reserve(items.size());
    for (auto&& item : items) {
      push_back(item);
    }
  }

  InplaceVector(const InplaceVector& other) {
    reserve(other.size_);
    for (auto&& item : other) {
      push_back(item);
    }
  }

  InplaceVector(InplaceVector&& other) noexcept {
    moveFrom(std::move(other));
  }

  InplaceVector& operator=(const InplaceVector& other) {
    if (&other == this) {
      return *this;
    }
    reset();
    reserve(other.size_);
    for (auto&& item : other) {
      push_back(item);
    }
    return *this;
  }

  InplaceVector& operator=(InplaceVector&& other) noexcept {
    if (&other == this) {
      return *this;
    }
    reset();
    moveFrom(std::move(other));
    return *this;
  }

  ~InplaceVector() {
    reset();
  }

  bool operator==(const InplaceVector& other) const {
    return size_ == other.size_ && std::equal(begin(), end(), other.begin());
  }

  bool operator<(const InplaceVector& other) const {
    return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
  }

  bool empty() const {
    return size_ == 0;
  }

  size_t size() const {
    return size_;
  }

  iterator begin() {
    return data();
  }

  iterator end() {
    return data() + size_;
  }

  const_iterator begin() const {
    return data();
  }

  const_iterator end() const {
    return data() + size_;
  }

  T& operator[](size_t i) {
    return data()[i];
  }

  const T& operator[](size_t i) const {
    return data()[i];
  }

  T& at(size_t i) {
    if (i >= size_) {
      throw std::out_of_range("InplaceVector::at");
    }
    return data()[i];
  }

  const T& at(size_t i) const {
    if (i >= size_) {
      throw std::out_of_range("InplaceVector::at");
    }
    return data()[i];
  }

  void reserve(size_t minCapacity) {
    if (minCapacity <= capacity_) {
      return;
    }
    grow(minCapacity);
  }

  void clear() {
    while (size_ != 0) {
      data()[--size_].~T();
    }
  }

  void push_back(const T& value) {
    emplaceBack(value);
  }

  void push_back(T&& value) {
    emplaceBack(std::move(value));
  }

  iterator insert(const_iterator pos, const T& value) {
    return emplace(pos, value);
  }

  iterator insert(const_iterator pos, T&& value) {
    return emplace(pos, std::move(value));
  }

  iterator erase(const_iterator pos) {
    auto index = static_cast<size_t>(pos - begin());
    auto* values = data();
    for (size_t i = index; i + 1 < size_; ++i) {
      values[i] = std::move(values[i + 1]);
    }
    values[size_ - 1].~T();
    --size_;
    return values + index;
  }

 private:
  using storage_type = std::aligned_storage_t<sizeof(T), alignof(T)>;

  iterator data() {
    return heap_ != nullptr ? heap_ : reinterpret_cast<T*>(inline_);
  }

  const_iterator data() const {
    return heap_ != nullptr ? heap_ : reinterpret_cast<const T*>(inline_);
  }

  bool isInline() const {
    return heap_ == nullptr;
  }

  template <typename U>
  void emplaceBack(U&& value) {
    reserve(size_ + 1);
    new (data() + size_) T{std::forward<U>(value)};
    ++size_;
  }

  template <typename U>
  iterator emplace(const_iterator pos, U&& value) {
    auto index = static_cast<size_t>(pos - begin());
    reserve(size_ + 1);
    auto* values = data();
    if (index == size_) {
      new (values + size_) T{std::forward<U>(value)};
      ++size_;
      return values + index;
    }

    T tmp{std::forward<U>(value)};
    new (values + size_) T{std::move(values[size_ - 1])};
    for (size_t i = size_ - 1; i > index; --i) {
      values[i] = std::move(values[i - 1]);
    }
    values[index] = std::move(tmp);
    ++size_;
    return values + index;
  }

  void grow(size_t minCapacity) {
    size_t newCapacity = capacity_ * 2;
    if (newCapacity < minCapacity) {
      newCapacity = minCapacity;
    }

    auto* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity));
    size_t i = 0;
    try {
      for (; i < size_; ++i) {
        new (newData + i) T{std::move(data()[i])};
      }
    } catch (...) {
      while (i != 0) {
        newData[--i].~T();
      }
      ::operator delete(newData);
      throw;
    }

    clear();
    if (!isInline()) {
      ::operator delete(heap_);
    }
    heap_ = newData;
    capacity_ = newCapacity;
    size_ = i;
  }

  void moveFrom(InplaceVector&& other) {
    if (other.isInline()) {
      reserve(other.size_);
      for (auto&& item : other) {
        push_back(std::move(item));
      }
      other.clear();
    } else {
      heap_ = other.heap_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.heap_ = nullptr;
      other.size_ = 0;
      other.capacity_ = InlineCapacity;
    }
  }

  void reset() {
    clear();
    if (!isInline()) {
      ::operator delete(heap_);
      heap_ = nullptr;
      capacity_ = InlineCapacity;
    }
  }

  alignas(T) storage_type inline_[InlineCapacity];
  T* heap_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = InlineCapacity;
};
