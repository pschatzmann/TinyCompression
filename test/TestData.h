#pragma once
// Deterministic test corpora.
#include <cstdint>
#include <vector>

namespace tc_test {

enum DataKind { kZeros, kRamp, kText, kRandom, kRuns, kAllBytes, kDataKinds };

inline uint32_t xorshift(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

inline std::vector<uint8_t> makeData(int kind, size_t n, uint32_t seed = 12345) {
  std::vector<uint8_t> v(n);
  uint32_t s = seed ? seed : 1;
  static const char* words[] = {"sensor", "value", "temp", "42", "ok", "tiny", "stream", "data "};
  size_t wi = 0;
  switch (kind) {
    case kZeros:
      break;
    case kRamp:
      for (size_t i = 0; i < n; ++i) v[i] = (uint8_t)i;
      break;
    case kText: {
      size_t i = 0;
      while (i < n) {
        const char* w = words[xorshift(s) % 8];
        for (const char* c = w; *c && i < n; ++c) v[i++] = (uint8_t)*c;
        if (i < n) v[i++] = ' ';
        ++wi;
      }
      break;
    }
    case kRandom:
      for (size_t i = 0; i < n; ++i) v[i] = (uint8_t)xorshift(s);
      break;
    case kRuns: {
      size_t i = 0;
      while (i < n) {
        uint8_t val = (uint8_t)xorshift(s);
        size_t len = 1 + xorshift(s) % 40;
        for (size_t k = 0; k < len && i < n; ++k) v[i++] = val;
      }
      break;
    }
    case kAllBytes:
      for (size_t i = 0; i < n; ++i) v[i] = (uint8_t)i;
      break;
  }
  (void)wi;
  return v;
}

}  // namespace tc_test
