#include "demangle.h"

#include <cxxabi.h>

#include <memory>

namespace detail {

std::string demangleImpl(const char* name) {
  int status = -4;

  std::unique_ptr<char, void (*)(void*)> res{
      abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};

  return status == 0 ? res.get() : name;
}

}  // namespace detail
