#pragma once
#include "../Codec.h"

namespace TinyCompression {

/// Delta codec: each byte is stored as the difference to the previous byte.
/// Differences in -7..+7 take a 4-bit code (diff+7); anything else uses the
/// escape code 15 followed by the full 8-bit difference. Smooth signals (sensor
/// readings, ramps, slowly changing values) shrink to about half their size.
class DeltaCodec : public Codec {
public:
  uint8_t id() const override { return 2; }
  const char* name() const override { return "Delta"; }

  size_t encode(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) override {
    detail::BitWriter bw(dst, cap);
    uint8_t prev = 0;
    for (size_t i = 0; i < n; ++i) {
      int8_t d = (int8_t)(uint8_t)(src[i] - prev);
      if (d >= -7 && d <= 7) {
        bw.put((uint32_t)(d + 7), 4);
      } else {
        bw.put(15, 4);
        bw.put((uint8_t)d, 8);
      }
      prev = src[i];
    }
    return bw.finish();
  }

  bool decode(const uint8_t* src, size_t n, uint8_t* dst, size_t dstLen) override {
    detail::BitReader br(src, n);
    uint8_t prev = 0;
    for (size_t o = 0; o < dstLen; ++o) {
      uint32_t code;
      if (!br.get(4, code)) return false;
      int d;
      if (code == 15) {
        uint32_t raw;
        if (!br.get(8, raw)) return false;
        d = (int8_t)(uint8_t)raw;
      } else {
        d = (int)code - 7;
      }
      prev = (uint8_t)(prev + d);
      dst[o] = prev;
    }
    return true;
  }
};

}  // namespace TinyCompression
