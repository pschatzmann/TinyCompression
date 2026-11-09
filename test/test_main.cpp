#include <cstdio>
#include "TestFramework.h"

int main() {
  for (const auto& t : tc_test::registry()) {
    int before = tc_test::failures();
    t.fn();
    std::printf("%-4s %s\n", tc_test::failures() == before ? "PASS" : "FAIL", t.name);
  }
  std::printf("\n%d checks, %d failed\n", tc_test::checks(), tc_test::failures());
  return tc_test::failures() == 0 ? 0 : 1;
}
