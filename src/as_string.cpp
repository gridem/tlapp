#include "as_string.h"

std::string asString(char c) {
  return std::string{"'"} + c + "'";
}

std::string asString(const char* c) {
  return c;
}

const std::string& asString(bool w) {
  static const std::string t{"true"};
  static const std::string f{"false"};
  return w ? t : f;
}

std::string asString(const std::string& t) {
  return t;
}
