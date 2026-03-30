#pragma once

#include <glog/logging.h>

#include "demangle.h"

#define DUMP(X) LOG(WARNING) << "DUMP " #X ": " << X;
#define TYPE(X) DUMP(demangle<X>())
#define DECLTYPE(X) TYPE(decltype(X))
#define DO(x)                \
  LOG(WARNING) << "DO: " #x; \
  x
