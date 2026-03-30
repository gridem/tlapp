#include "expression.h"

#include <gtest/gtest.h>

#include "var.h"

namespace test {

TEST(Expression, Test) {
  Var<int> i{"i"};
  Expression<int> e1 = i.toExpression();
  Expression<int> e2 = e1;
}

}  // namespace test
