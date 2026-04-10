#include "descriptor.h"

#include "as_string.h"
#include "error.h"

Descriptor::Descriptor(const char* varName) : name{varName} {}

void Descriptor::validate() const {
  if (index > 100) {
    throw VarValidationError("Invalid index");
  }
}

bool Descriptor::isAdded() const {
  validate();
  return index >= 0;
}

std::string Descriptor::toString() const {
  return asStringQuote(name);
}

std::string Descriptor::toStringNext() const {
  return asStringQuote(asString(name, "++"));
}
