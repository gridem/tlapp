#pragma once

#include <stdexcept>
#include <string>

#define DECL_ERROR(D_name, D_base, D_desc)                                 \
  struct D_name : D_base {                                                 \
    D_name() : D_base(D_desc) {}                                           \
    explicit D_name(const std::string& msg) : D_base(D_desc ": " + msg) {} \
  };

DECL_ERROR(TlappError, std::runtime_error, "TLA++ error")
DECL_ERROR(EngineError, TlappError, "Engine error")
DECL_ERROR(EngineStop, EngineError, "Engine stopped")
DECL_ERROR(EnsureError, EngineError, "Ensure error")
DECL_ERROR(LivenessError, EngineError, "Liveness error")
DECL_ERROR(EngineInitError, EngineError, "Engine init error")
DECL_ERROR(EngineBooleanError, EngineError, "Engine boolean expression error")
DECL_ERROR(ExpressionError, TlappError, "Expression error")
DECL_ERROR(VariableError, TlappError, "Variable error")
DECL_ERROR(VarInitError, VariableError, "Variable initialization error")
DECL_ERROR(VarValidationError, VariableError, "Variable validation error")
