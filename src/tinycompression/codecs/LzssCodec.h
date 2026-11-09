#pragma once
#include "../Codec.h"

namespace TinyCompression {

/// Classic LZSS: every item is preceded by a flag bit.
///   1 + 8 bits            -> literal byte
///   0 + 12 bits + 4 bits  -> back-reference: offset (1..4095), length-3 (3..18)
/// Matches are searched within the current block only (brute force).
class LzssCodec : public Codec {
public:
  uint8_t id() const override { return 3; }
  const char* name() const override { return "LZSS"; }

  size_t encode(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) override {
    detail::BitWriter bw(dst, cap);
    size_t i = 0;
    while (i < n) {
      size_t maxLen = detail::minOf((size_t)kMaxMatch, n - i);
      size_t maxOff = detail::minOf((size_t)kMaxOffset, i);
      size_t bestLen = 0;
      size_t bestOff = 0;
      for (size_t off = 1; off <= maxOff && bestLen < maxLen; ++off) {
        size_t l = 0;
        while (l < maxLen && src[i - off + l] == src[i + l]) ++l;
        if (l > bestLen) {
          bestLen = l;
          bestOff = off;
        }
      }
      if (bestLen >= (size_t)kMinMatch) {
        bw.putBit(0);
        bw.put((uint32_t)bestOff, 12);
        bw.put((uint32_t)(bestLen - kMinMatch), 4);
        i += bestLen;
      } else {
        bw.putBit(1);
        bw.put(src[i], 8);
        ++i;
      }
    }
    return bw.finish();
  }

  bool decode(const uint8_t* src, size_t n, uint8_t* dst, size_t dstLen) override {
    detail::BitReader br(src, n);
    size_t o = 0;
    while (o < dstLen) {
      uint32_t flag;
      if (!br.get(1, flag)) return false;
      if (flag) {
        uint32_t v;
        if (!br.get(8, v)) return false;
        dst[o++] = (uint8_t)v;
        continue;
      }
      uint32_t off;
      uint32_t len;
      if (!br.get(12, off) || !br.get(4, len)) return false;
      size_t mlen = (size_t)len + kMinMatch;
      if (off == 0 || off > o || mlen > dstLen - o) return false;
      for (size_t k = 0; k < mlen; ++k, ++o) dst[o] = dst[o - off];
    }
    return true;
  }

private:
  enum { kMinMatch = 3, kMaxMatch = 18, kMaxOffset = 4095 };
};

}  // namespace TinyCompression
