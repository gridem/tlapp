#pragma once

#include <string>

enum class LogicState {
  Init = 0,
  Next = 1,
};

using VarIndex = int;  // position in a list of vars

struct Descriptor {
  explicit Descriptor(const char* varName);

  const char* name = nullptr;

  // Index in context. -1 means unregistered value.
  // Can be changed on lazy registration.
  VarIndex index = -1;

  // Sanity check.
  void validate() const;
  bool isAdded() const;

  std::string toString() const;
  std::string toStringNext() const;

  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;
  Descriptor(Descriptor&&) = delete;
  Descriptor& operator=(Descriptor&&) = delete;
};

template <LogicState Logic>
struct DiscriminatorDescriptor : public Descriptor {
  using Descriptor::Descriptor;
};