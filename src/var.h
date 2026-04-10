#pragma once

#include <memory>

#include "context.h"
#include "error.h"
#include "tag.h"

template <typename T>
struct Var : assignment_tag_type, expression_tag_type, var_tag_type {
  // Var expression wrapper.
  template <LogicState I>
  struct Expression : assignment_tag_type, expression_tag_type {
    explicit Expression(Descriptor* descriptor) : descriptor_{descriptor} {}

    auto& getRef(Context& ctx) const { return ctx.getRef<T, I>(*descriptor_); }

    tname(U) bool assignTo(Context& ctx, U&& u) const {
      auto&& val = getRef(ctx);
      if (!val) {
        val = fwd(u);
        return true;
      }
      return *val == u;
    }

    const T& operator()(Context& ctx) const {
      auto&& val = getRef(ctx);
      if (!val) {
        throw VarInitError(asString("Variable ", *descriptor_, " must be initialized"));
      }
      return *val;
    }

    static constexpr LogicState getState() { return I; }

   private:
    Descriptor* descriptor_;
  };

  explicit Var(const char* name) : descriptor_{name} {}

  const T& operator()(Context& ctx) { return toExpression()(ctx); }

  std::optional<T>& getRef(Context& ctx) {
    return ctx.getRef<T, LogicState::Init>(descriptor_);
  }

  std::optional<T>& getRefNext(Context& ctx) {
    return ctx.getRef<T, LogicState::Next>(descriptor_);
  }

  let toExpression() { return toTemporalExpression<LogicState::Init>(); }
  let operator++(int) { return toTemporalExpression<LogicState::Next>(); }

 private:
  template <LogicState I>
  let toTemporalExpression() {
    return Expression<I>{&descriptor_};
  }

  Descriptor descriptor_;
};
