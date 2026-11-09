#pragma once
#include "../Codec.h"

namespace TinyCompression {

/// Run-length codec (PackBits style).
/// Header byte h: h < 128 -> the next h+1 bytes are literals;
///                h >= 128 -> the next byte is repeated (h-128)+2 times.
class RleCodec : public Codec {
public:
  uint8_t id() const override { return 1; }
  const char* name() const override { return "RLE"; }

  size_t encode(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) override {
    detail::ByteWriter out(dst, cap);
    size_t i = 0;
    while (i < n) {
      size_t run = 1;
      while (i + run < n && run < 129 && src[i + run] == src[i]) ++run;
      if (run >= 2) {
        out.put((uint8_t)(128 + run - 2));
        out.put(src[i]);
        i += run;
        continue;
      }
      // Literal group: stop before the next run of two or more equal bytes.
      size_t start = i;
      size_t lit = 0;
      while (i < n && lit < 128 && !(i + 1 < n && src[i] == src[i + 1])) {
        ++i;
        ++lit;
      }
      out.put((uint8_t)(lit - 1));
      out.copy(src + start, lit);
    }
    return out.size();
  }

  bool decode(const uint8_t* src, size_t n, uint8_t* dst, size_t dstLen) override {
    size_t i = 0;
    size_t o = 0;
    while (o < dstLen) {
      if (i >= n) return false;
      uint8_t h = src[i++];
      if (h < 128) {
        size_t lit = (size_t)h + 1;
        if (lit > n - i || lit > dstLen - o) return false;
        memcpy(dst + o, src + i, lit);
        i += lit;
        o += lit;
      } else {
        size_t run = (size_t)(h - 128) + 2;
        if (i >= n || run > dstLen - o) return false;
        memset(dst + o, src[i++], run);
        o += run;
      }
    }
    return i == n;
  }
};

}  // namespace TinyCompression
