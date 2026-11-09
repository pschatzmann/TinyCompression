#pragma once
#include "../Codec.h"

namespace TinyCompression {

/// LZ4 block format (no frame header). Each sequence is:
///   token (literal length in high nibble, match length-4 in low nibble),
///   optional length extension bytes, literals, 2-byte LE offset, optional
///   match length extension. The last sequence contains only literals.
/// Encoder rules follow the LZ4 spec: the last 5 bytes are literals and the
/// last match starts at least 12 bytes before the end of the block.
class Lz4Codec : public Codec {
public:
  uint8_t id() const override { return 5; }
  const char* name() const override { return "LZ4"; }

  size_t encode(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) override {
    if (n == 0 || n > (size_t)kMaxInput) return 0;
    memset(_table, 0, sizeof(_table));
    detail::ByteWriter out(dst, cap);

    size_t anchor = 0;
    size_t ip = 0;
    if (n >= 13) {
      while (ip + 12 <= n) {
        uint32_t seq = read32(src + ip);
        uint32_t h = (seq * 2654435761u) >> (32 - kHashBits);
        size_t ref = _table[h];  // stores position + 1, 0 = empty
        _table[h] = (uint16_t)(ip + 1);
        if (ref != 0) {
          ref -= 1;
          if (ip - ref <= 0xFFFF && read32(src + ref) == seq) {
            size_t mlen = 4;
            while (ip + mlen < n - 5 && src[ref + mlen] == src[ip + mlen]) ++mlen;
            emitSequence(out, src + anchor, ip - anchor, true, ip - ref, mlen);
            ip += mlen;
            anchor = ip;
            continue;
          }
        }
        ++ip;
      }
    }
    emitSequence(out, src + anchor, n - anchor, false, 0, 0);
    return out.size();
  }

  bool decode(const uint8_t* src, size_t n, uint8_t* dst, size_t dstLen) override {
    size_t ip = 0;
    size_t o = 0;
    for (;;) {
      if (ip >= n) return false;
      uint8_t token = src[ip++];

      size_t lit = token >> 4;
      if (lit == 15 && !readLength(src, n, ip, lit)) return false;
      if (lit > n - ip || lit > dstLen - o) return false;
      memcpy(dst + o, src + ip, lit);
      ip += lit;
      o += lit;

      if (ip == n) break;  // last sequence has no match

      if (n - ip < 2) return false;
      size_t offset = (size_t)src[ip] | ((size_t)src[ip + 1] << 8);
      ip += 2;

      size_t ml = token & 15;
      if (ml == 15 && !readLength(src, n, ip, ml)) return false;
      ml += 4;
      if (offset == 0 || offset > o || ml > dstLen - o) return false;
      for (size_t k = 0; k < ml; ++k, ++o) dst[o] = dst[o - offset];
    }
    return o == dstLen;
  }

private:
  enum { kHashBits = 10, kMaxInput = 32768 };

  static uint32_t read32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
  }

  static void writeLength(detail::ByteWriter& out, size_t rem) {
    while (rem >= 255) {
      out.put(255);
      rem -= 255;
    }
    out.put((uint8_t)rem);
  }

  static void emitSequence(detail::ByteWriter& out, const uint8_t* lits, size_t litLen,
                           bool hasMatch, size_t offset, size_t matchLen) {
    size_t ml = hasMatch ? matchLen - 4 : 0;
    uint8_t token = (uint8_t)(((litLen < 15 ? litLen : 15) << 4) | (ml < 15 ? ml : 15));
    out.put(token);
    if (litLen >= 15) writeLength(out, litLen - 15);
    out.copy(lits, litLen);
    if (hasMatch) {
      out.put((uint8_t)(offset & 0xFF));
      out.put((uint8_t)(offset >> 8));
      if (ml >= 15) writeLength(out, ml - 15);
    }
  }

  static bool readLength(const uint8_t* src, size_t n, size_t& ip, size_t& value) {
    uint8_t b;
    do {
      if (ip >= n) return false;
      b = src[ip++];
      value += b;
    } while (b == 255);
    return true;
  }

  uint16_t _table[1 << kHashBits];
};

}  // namespace TinyCompression
