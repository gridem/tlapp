#include <gtest/gtest.h>

#include "extractor.h"
#include "field.h"

namespace test {

struct_fields(Sab, (int, a), (int, b));

TEST(Mem, Get) {
  Var<Sab> ab("Sab");

  Context ctx;
  auto ea = get_mem(ab, a);
  auto eb = get_mem(ab, b);

  ab.getRef(ctx) = {{1, 2}};
  EXPECT_EQ(1, ea(ctx));
  EXPECT_EQ(2, eb(ctx));
}

TEST(Mem, Creator) {
  Var<int> x{"x"};

  auto eab = creator<Sab>(x, 2);

  Context ctx;
  x.getRef(ctx) = 1;

  auto ab = eab(ctx);
  EXPECT_EQ(1, ab.a);
  EXPECT_EQ(2, ab.b);
}

}  // namespace test
