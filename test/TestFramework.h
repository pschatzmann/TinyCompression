#pragma once
// Minimal test framework: no external dependencies.
#include <cstdio>
#include <vector>

namespace tc_test {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& failures() {
  static int count = 0;
  return count;
}

inline int& checks() {
  static int count = 0;
  return count;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

}  // namespace tc_test

#define TEST_CASE(name)                                             \
  static void name();                                               \
  static tc_test::Registrar name##_registrar(#name, &name);         \
  static void name()

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++tc_test::checks();                                                         \
    if (!(cond)) {                                                               \
      ++tc_test::failures();                                                     \
      std::fprintf(stderr, "  %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                            \
  } while (0)
