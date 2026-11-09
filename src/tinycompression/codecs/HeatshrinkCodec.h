#pragma once
#include "../Codec.h"

namespace TinyCompression {

/// Heatshrink-style LZSS with configurable window and lookahead sizes.
///   1 + 8 bits                        -> literal byte
///   0 + windowBits + lookaheadBits    -> back-reference: (distance-1), (length-1)
/// Back-references are only emitted when they are smaller than the literals they replace.
/// The bit layout is compatible with the reference heatshrink library (verified
/// by decoding its output in the test suite). Both sides must use the same
/// window/lookahead parameters.
class HeatshrinkCodec : public Codec {
public:
  /// windowBits is clamped to 4..13, lookaheadBits to 2..windowBits-1.
  HeatshrinkCodec(uint8_t windowBits = 8, uint8_t lookaheadBits = 4)
    : _w(detail::clampBits(windowBits, 4, 13)),
      _l(detail::clampBits(lookaheadBits, 2, _w - 1)) {}

  uint8_t id() const override { return 4; }
  const char* name() const override { return "Heatshrink"; }
  uint8_t windowBits() const { return _w; }
  uint8_t lookaheadBits() const { return _l; }

  size_t encode(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) override {
    detail::BitWriter bw(dst, cap);
    const size_t window = (size_t)1 << _w;
    const size_t maxCount = (size_t)1 << _l;
    size_t i = 0;
    while (i < n) {
      size_t maxLen = detail::minOf(maxCount, n - i);
      size_t maxDist = detail::minOf(window, i);
      size_t bestLen = 0;
      size_t bestDist = 0;
      for (size_t d = 1; d <= maxDist && bestLen < maxLen; ++d) {
        size_t l = 0;
        while (l < maxLen && src[i - d + l] == src[i + l]) ++l;
        if (l > bestLen) {
          bestLen = l;
          bestDist = d;
        }
      }
      // A back-reference costs 1+W+L bits; a literal costs 9 bits.
      if (bestLen >= 2 && bestLen * 9 > (size_t)(1 + _w + _l)) {
        bw.putBit(0);
        bw.put((uint32_t)(bestDist - 1), _w);
        bw.put((uint32_t)(bestLen - 1), _l);
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
      uint32_t dist;
      uint32_t len;
      if (!br.get(_w, dist) || !br.get(_l, len)) return false;
      size_t d = (size_t)dist + 1;
      size_t mlen = (size_t)len + 1;
      if (d > o || mlen > dstLen - o) return false;
      for (size_t k = 0; k < mlen; ++k, ++o) dst[o] = dst[o - d];
    }
    return true;
  }

private:
  uint8_t _w;
  uint8_t _l;
};

}  // namespace TinyCompression
