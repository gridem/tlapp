#pragma once

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <new>

#include "evaluate.h"
#include "expression.h"

namespace detail {

tname(T_expr) struct CheckBranchOp {
  using expr_type = PreparedExpression<T_expr>;

  tname(T) explicit CheckBranchOp(T&& expr) : expr_{detail::prepare(fwd(expr))} {}

  bool apply(Context& ctx) const {
    return detail::extract(ctx, expr_);
  }

 private:
  expr_type expr_;
};

tname(T_l, T_r) struct AssignBranchOp {
  tname(T1, T2) AssignBranchOp(T1&& l, T2&& r) : l_{fwd(l)}, r_{fwd(r)} {}

  bool apply(Context& ctx) const {
    return l_.assignTo(ctx, r_);
  }

 private:
  std::decay_t<T_l> l_;
  std::decay_t<T_r> r_;
};

}  // namespace detail

struct BranchOp {
  using Apply = bool (*)(Context& ctx, const void* op);
  using Copy = void (*)(const BranchOp& src, BranchOp& dst);
  using Move = void (*)(BranchOp& src, BranchOp& dst);
  using Destroy = void (*)(BranchOp& op);

  struct Ops {
    Apply apply;
    Copy copy;
    Move move;
    Destroy destroy;
  };

  static constexpr size_t kInlineSize = sizeof(void*) * 4;

  BranchOp() = default;

  BranchOp(const BranchOp& other) {
    other.copyTo(*this);
  }

  BranchOp(BranchOp&& other) {
    other.moveTo(*this);
  }

  BranchOp& operator=(const BranchOp& other) {
    if (&other == this) {
      return *this;
    }
    reset();
    other.copyTo(*this);
    return *this;
  }

  BranchOp& operator=(BranchOp&& other) {
    if (&other == this) {
      return *this;
    }
    reset();
    other.moveTo(*this);
    return *this;
  }

  ~BranchOp() {
    reset();
  }

  template <typename T_op,
      std::enable_if_t<!std::is_same_v<std::decay_t<T_op>, BranchOp>, int> = 0>
  explicit BranchOp(T_op&& op) {
    emplace<std::decay_t<T_op>>(fwd(op));
  }

  bool apply(Context& ctx) const {
    return ops_->apply(ctx, get());
  }

 private:
  template <typename T>
  static constexpr bool kIsInline =
      sizeof(T) <= kInlineSize && alignof(T) <= alignof(std::max_align_t);

  template <typename T>
  static bool applyImpl(Context& ctx, const void* op) {
    return static_cast<const T*>(op)->apply(ctx);
  }

  template <typename T>
  static void copyImpl(const BranchOp& src, BranchOp& dst) {
    if constexpr (kIsInline<T>) {
      new (&dst.storage_) T{*static_cast<const T*>(src.get())};
      dst.ptr_ = nullptr;
    } else {
      dst.ptr_ = new T{*static_cast<const T*>(src.get())};
    }
    dst.ops_ = &opsFor<T>();
  }

  template <typename T>
  static void moveImpl(BranchOp& src, BranchOp& dst) {
    if constexpr (kIsInline<T>) {
      new (&dst.storage_) T{std::move(*static_cast<T*>(src.getMutable()))};
      static_cast<T*>(src.getMutable())->~T();
      dst.ptr_ = nullptr;
    } else {
      dst.ptr_ = src.ptr_;
      src.ptr_ = nullptr;
    }
    dst.ops_ = &opsFor<T>();
    src.ops_ = nullptr;
  }

  template <typename T>
  static void destroyImpl(BranchOp& op) {
    if constexpr (kIsInline<T>) {
      static_cast<T*>(op.getMutable())->~T();
    } else {
      delete static_cast<T*>(op.ptr_);
      op.ptr_ = nullptr;
    }
    op.ops_ = nullptr;
  }

  template <typename T>
  static const Ops& opsFor() {
    static constexpr Ops ops = {
        &applyImpl<T>, &copyImpl<T>, &moveImpl<T>, &destroyImpl<T>};
    return ops;
  }

  template <typename T, typename U>
  void emplace(U&& op) {
    if constexpr (kIsInline<T>) {
      new (&storage_) T{fwd(op)};
      ptr_ = nullptr;
    } else {
      ptr_ = new T{fwd(op)};
    }
    ops_ = &opsFor<T>();
  }

  const void* get() const {
    return ptr_ == nullptr ? static_cast<const void*>(&storage_) : ptr_;
  }

  void* getMutable() {
    return ptr_ == nullptr ? static_cast<void*>(&storage_) : ptr_;
  }

  void copyTo(BranchOp& dst) const {
    if (ops_ != nullptr) {
      ops_->copy(*this, dst);
    }
  }

  void moveTo(BranchOp& dst) {
    if (ops_ != nullptr) {
      ops_->move(*this, dst);
    }
  }

  void reset() {
    if (ops_ != nullptr) {
      ops_->destroy(*this);
    }
  }

  alignas(std::max_align_t) unsigned char storage_[kInlineSize];
  void* ptr_ = nullptr;
  const Ops* ops_ = nullptr;
};

// Branch of checks and assignments combined with `&&`.
struct BranchResult {
  using value_type = BranchOp;
  using iterator = BranchOp*;
  using const_iterator = const BranchOp*;

  static constexpr size_t kInlineCapacity = 4;

  BranchResult() = default;

  BranchResult(std::initializer_list<BranchOp> ops) {
    append(ops.begin(), ops.end());
  }

  BranchResult(const BranchResult& other) {
    append(other.begin(), other.end());
  }

  BranchResult(BranchResult&& other) noexcept {
    moveFrom(std::move(other));
  }

  BranchResult& operator=(const BranchResult& other) {
    if (&other == this) {
      return *this;
    }
    reset();
    append(other.begin(), other.end());
    return *this;
  }

  BranchResult& operator=(BranchResult&& other) noexcept {
    if (&other == this) {
      return *this;
    }
    reset();
    moveFrom(std::move(other));
    return *this;
  }

  ~BranchResult() {
    reset();
  }

  tname(T_expr) static BranchResult fromCheck(T_expr&& expr) {
    return BranchResult{BranchOp{detail::CheckBranchOp<T_expr>{fwd(expr)}}};
  }

  tname(T_l, T_r) static BranchResult fromAssign(T_l&& l, T_r&& r) {
    return BranchResult{BranchOp{
        detail::AssignBranchOp<std::decay_t<T_l>, std::decay_t<T_r>>{fwd(l, r)}}};
  }

  size_t size() const {
    return size_;
  }

  void reserve(size_t size) {
    if (size > capacity_) {
      grow(size);
    }
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

  BranchOp& operator[](size_t i) {
    return data()[i];
  }

  const BranchOp& operator[](size_t i) const {
    return data()[i];
  }

  template <typename T_iter>
  void insert(iterator pos, T_iter first, T_iter last) {
    if (pos != end()) {
      throw ExpressionError("BranchResult only supports append insert");
    }
    append(first, last);
  }

  // Should be used for testing purposes only because it represents a set of
  // possible combinations.
  bool operator()(Context& ctx) const {
    for (auto&& op : *this) {
      if (!op.apply(ctx)) {
        return false;
      }
    }
    return true;
  }

 private:
  BranchOp* data() {
    return heap_ != nullptr ? heap_ : reinterpret_cast<BranchOp*>(storage_);
  }

  const BranchOp* data() const {
    return heap_ != nullptr ? heap_ : reinterpret_cast<const BranchOp*>(storage_);
  }

  bool isInline() const {
    return heap_ == nullptr;
  }

  template <typename T_iter>
  void append(T_iter first, T_iter last) {
    auto count = static_cast<size_t>(std::distance(first, last));
    if (count == 0) {
      return;
    }
    reserve(size_ + count);
    auto out = end();
    for (; first != last; ++first, ++out) {
      new (out) BranchOp{*first};
    }
    size_ += count;
  }

  void clear() {
    while (size_ != 0) {
      data()[--size_].~BranchOp();
    }
  }

  void reset() {
    clear();
    if (!isInline()) {
      ::operator delete(heap_);
      heap_ = nullptr;
      capacity_ = kInlineCapacity;
    }
  }

  void grow(size_t minCapacity) {
    size_t newCapacity = capacity_ * 2;
    if (newCapacity < minCapacity) {
      newCapacity = minCapacity;
    }
    auto* newData =
        static_cast<BranchOp*>(::operator new(sizeof(BranchOp) * newCapacity));
    size_t i = 0;
    try {
      for (; i < size_; ++i) {
        new (&newData[i]) BranchOp{std::move(data()[i])};
      }
    } catch (...) {
      while (i != 0) {
        newData[--i].~BranchOp();
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

  void moveFrom(BranchResult&& other) {
    if (other.isInline()) {
      append(
          std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
      other.clear();
    } else {
      heap_ = other.heap_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.heap_ = nullptr;
      other.size_ = 0;
      other.capacity_ = kInlineCapacity;
    }
  }

  alignas(BranchOp) unsigned char storage_[sizeof(BranchOp) * kInlineCapacity];
  BranchOp* heap_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = kInlineCapacity;
};

// Set of branches combined with `||`.
struct LogicResult : std::vector<BranchResult> {
  using std::vector<BranchResult>::vector;

  tname(T_expr) static LogicResult fromRaw(T_expr&& expr) {
    return {BranchResult::fromCheck(fwd(expr))};
  }

  tname(T_l, T_r) static LogicResult fromAssign(T_l&& l, T_r&& r) {
    return {BranchResult::fromAssign(fwd(l, r))};
  }

  // Should be used for testing purposes only because it represents a set of
  // possible combinations.
  bool evaluate(Context& ctx) const {
    for (auto&& branch : *this) {
      if (branch(ctx)) {
        return true;
      }
    }
    return false;
  }
};

// Represents generic boolean result.
// It could be raw boolean,
// or matrix of possible combinations with assignments.
struct BooleanResult : std::variant<bool, LogicResult> {
  using std::variant<bool, LogicResult>::variant;

  tname(B) static BooleanResult fromRaw(B&& b) {
    return LogicResult::fromRaw(fwd(b));
  }

  // Should be used for testing purposes only because it represents a set of
  // possible combinations.
  bool evaluate(Context& ctx) const {
    return std::visit(
        [&ctx] lam_arg(v) {
          if_eq(v, bool) {
            return v;
          } else {
            return v.evaluate(ctx);
          }
        },
        *this);
  }
};

using Boolean = Expression<BooleanResult>;

namespace detail {

template <>
struct PredicateResult<bool> {
  static bool apply(bool result) {
    return result;
  }
};

template <>
struct PredicateNeedsCheck<BooleanResult> : std::true_type {};

template <>
struct PredicateResult<BooleanResult> {
  static bool apply(const BooleanResult& result) {
    if (auto b = std::get_if<bool>(&result)) {
      return *b;
    }
    throw EngineBooleanError("Invalid boolean value type: must be simple bool");
  }
};

fun(isAssign, var, ctx) {
  /*
    Possible context options:
      - init
      - next
      - init/next+check
    Assignments types:
      - eqTo: just comparison without assignment
      - assignTo: assignment with check
    Combinations:
      - check: return eq always
      - init state + init var: assign
      - next state + next var: assign
      - init state + next var: error, will be checked inside context
      - next state + init var: eq
  */
  if (ctx.isCheck()) {
    return false;
  }
  return var.getState() == ctx.getState();
}

fun(assignTo, ctx, l, r) {
  auto rval = extract(ctx, fwd(r));
  return BooleanResult{LogicResult::fromAssign(fwd(l), std::move(rval))};
}

fun(eqTo, ctx, l, r) {
  return BooleanResult{extract(ctx, fwd(l)) == extract(ctx, fwd(r))};
}

let assignOp = lam(ctx, l, r) {
  return isAssign(l, ctx) ? assignTo(ctx, fwd(l, r)) : eqTo(ctx, fwd(l, r));
};

fun(resultOf, t) {
  if_is(t, lazy) {
    return t();
  } else {
    return fwd(t);
  }
}

tname(L, R) using BinaryBooleanResultType =
    std::conditional_t<is_eq<L, R>, std::decay_t<L>, BooleanResult>;

static_assert(is_eq<bool, BinaryBooleanResultType<bool, bool>>);
static_assert(
    is_eq<BooleanResult, BinaryBooleanResultType<BooleanResult, BooleanResult>>);
static_assert(is_eq<LogicResult, BinaryBooleanResultType<LogicResult, LogicResult>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<bool, BooleanResult>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<BooleanResult, bool>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<LogicResult, bool>>);
static_assert(is_eq<BooleanResult, BinaryBooleanResultType<bool, LogicResult>>);

fun(binaryBooleanOp, impl, l, r) {
  // If any of lam_arg is bool, evaluate impl with first lam_arg as bool.
  if_eq(resultOf(fwd(l)), bool) {
    return impl(fwd(l, r));
  } else if_eq(resultOf(fwd(r)), bool) {
    return impl(fwd(r, l));
  } else if_eq(resultOf(fwd(l)), BooleanResult) {
    // Visit l and try again.
    return std::visit(
        [&r, &impl] lam_arg(
            v) -> BooleanResult { return binaryBooleanOp(fwd(impl, v, r)); },
        resultOf(fwd(l)));
  } else if_eq(resultOf(fwd(r)), BooleanResult) {
    // extract right.
    return binaryBooleanOp(fwd(impl, r, l));
  } else {
    return impl(fwd(r, l));
  }
}

fun(concatBranches, l, r) {
  using namespace std;
  if_is(l, rvalue_reference_v) {
    l.reserve(l.size() + r.size());
    l.insert(l.end(), r.begin(), r.end());
    return fwd(l);
  } else {
    BranchResult branch = l;
    branch.reserve(branch.size() + r.size());
    branch.insert(branch.end(), r.begin(), r.end());
    return branch;
  }
}

fun(appendBranch, l, r) {
  l.reserve(l.size() + r.size());
  l.insert(l.end(), r.begin(), r.end());
}

fun(appendLogic, l, r) {
  using namespace std;
  if_is(r, rvalue_reference_v) {
    if (r.size() == 1) {
      l.push_back(std::move(r[0]));
    } else {
      l.insert(l.end(), make_move_iterator(r.begin()), make_move_iterator(r.end()));
    }
  } else {
    if (r.size() == 1) {
      l.push_back(r[0]);
    } else {
      l.insert(l.end(), r.begin(), r.end());
    }
  }
}

fun(concatVectors, l, r) {
  using namespace std;
  if_is(l, rvalue_reference_v) {
    appendLogic(l, r);
    return fwd(l);
  } else {
    auto v = l;
    v.reserve(v.size() + r.size());
    appendLogic(v, r);
    return v;
  }
}

fun(mulVectorsImpl, ls, rs) {
  LogicResult res;
  res.reserve(ls.size() * rs.size());
  for (auto&& l : ls) {
    for (auto&& r : rs) {
      res.push_back(l);
      appendBranch(res.back(), r);
    }
  }
  return res;
}

fun(mulVectors, ls, rs) {
  using namespace std;
  if_is(ls, rvalue_reference_v) {
    if (ls.size() == 1 && rs.size() == 1) {
      appendBranch(ls[0], rs[0]);
      return LogicResult{std::move(ls)};
    } else {
      return mulVectorsImpl(fwd(ls, rs));
    }
  } else {
    return mulVectorsImpl(fwd(ls, rs));
  }
}

fun(orImpl, l, r) {
  if_eq(resultOf(fwd(l)), bool) {
    return resultOf(fwd(l)) ? BinaryBooleanResultType<decltype(resultOf(fwd(l))),
                                  decltype(resultOf(fwd(r)))>{true}
                            : resultOf(fwd(r));
  } else {
    return concatVectors(resultOf(fwd(l)), resultOf(fwd(r)));
  }
}

fun(andImpl, l, r) {
  if_eq(resultOf(fwd(l)), bool) {
    return resultOf(fwd(l)) ? BinaryBooleanResultType<decltype(resultOf(fwd(l))),
                                  decltype(resultOf(fwd(r)))>{resultOf(fwd(r))}
                            : false;
  } else {
    return mulVectors(resultOf(fwd(l)), resultOf(fwd(r)));
  }
}

fun(orOp, l, r) {
  return binaryBooleanOp(as_lam(orImpl), fwd(l, r));
}

fun(andOp, l, r) {
  return binaryBooleanOp(as_lam(andImpl), fwd(l, r));
}

}  // namespace detail

inline bool operator!(const LogicResult&) {
  throw ExpressionError("Negative operation cannot be applied to logic result");
}

inline bool operator!(const BooleanResult& b) {
  return std::visit(lam_in(v, !v), b);
}

fun_if(operator==, is_any_of(is_expression, T_l, T_r), l, r) {
  if_is(l, assignment) {
    // Left argument can be used in special assignment procedure.
    return evaluate_ctx(detail::assignOp, fwd(l, r));
  } else {
    // If assignment is not allowed, do just simple comparison.
    return evaluator(l == r, l, r);
  }
}

fun_if(operator||, is_any_of(is_expression, T_l, T_r), l, r) {
  return evaluator_lazy_fun(detail::orOp, l, r);
}

fun_if(operator&&, is_any_of(is_expression, T_l, T_r), l, r) {
  return evaluator_lazy_fun(detail::andOp, l, r);
}
