#pragma once

#include "boolean.h"
#include "evaluate.h"
#include "var.h"

// Extracts member from expression.
// D_e is expression.
// D_mem is a data member or accessor expression (not a call).
#define get_mem(D_e, D_mem) evaluate(lam_in(e, e.D_mem), fwd(D_e))

// Extract value from index-based containers. E.g. vectors or maps.
fun(at, e, i) { return evaluator(fwd(e).at(fwd(i)), e, i); }

// Assigns new value for index-based containers. E.g. vectors or maps.
// at(map, index, value) -> map[index] = value
fun(at, var, index, newValue) {
  return evaluate(
      lam(var, index, newValue) {
        auto copy = fwd(var);
        copy[fwd(index)] = fwd(newValue);
        return copy;
      },
      fwd(var, index, newValue));
}

// Mutates the variable to new value using `get`.
fun(mut, var, f) { return var++ == evaluate(fwd(f), var); }

fun(mutAt, var, index, newValue) {
  return var++ == at(var, fwd(index, newValue));
}

// Creates an instance of type T with parameters.
// Example: creator<T>(a, b)
tname(T, ... V) auto creator(V&&... v) {
  return evaluate(lam_in(... u, T(fwd(u)...)), fwd(v)...);
}
