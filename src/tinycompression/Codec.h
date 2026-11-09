#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace TinyCompression {

/// Base class of all compression codecs. A codec works on one block of data at
/// a time; the stream classes take care of splitting the data into blocks.
class Codec {
public:
  virtual ~Codec() {}

  /// Unique identifier stored in every frame (1..255). 0 is reserved for stored blocks.
  virtual uint8_t id() const = 0;
  virtual const char* name() const = 0;

  /// Compresses src into dst. Returns the number of bytes written, or 0 if the
  /// result does not fit into dstCap (or the input cannot be encoded).
  virtual size_t encode(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstCap) = 0;

  /// Decompresses src into exactly dstLen bytes. Returns false on malformed input.
  virtual bool decode(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen) = 0;
};

namespace detail {

inline size_t minOf(size_t a, size_t b) { return a < b ? a : b; }

inline uint8_t clampBits(int v, int lo, int hi) {
  return (uint8_t)(v < lo ? lo : (v > hi ? hi : v));
}

/// Writes bits MSB first into a fixed buffer. Overflow is remembered in ok().
class BitWriter {
public:
  BitWriter(uint8_t* dst, size_t cap) : _dst(dst), _cap(cap), _pos(0), _acc(0), _n(0), _ok(true) {}

  void putBit(uint8_t bit) {
    _acc = (uint8_t)((_acc << 1) | (bit & 1));
    if (++_n == 8) emit();
  }

  void put(uint32_t value, uint8_t bits) {
    for (int i = bits - 1; i >= 0; --i) putBit((uint8_t)((value >> i) & 1));
  }

  /// Pads the last partial byte with zeros and returns the encoded size (0 on overflow).
  size_t finish() {
    if (_n) {
      _acc = (uint8_t)(_acc << (8 - _n));
      emit();
    }
    return _ok ? _pos : 0;
  }

private:
  void emit() {
    if (_pos < _cap) {
      _dst[_pos] = _acc;
    } else {
      _ok = false;
    }
    ++_pos;
    _acc = 0;
    _n = 0;
  }

  uint8_t* _dst;
  size_t _cap;
  size_t _pos;
  uint8_t _acc;
  uint8_t _n;
  bool _ok;
};

/// Reads bits MSB first. get() returns false when the input is exhausted.
class BitReader {
public:
  BitReader(const uint8_t* src, size_t len) : _src(src), _bits(len * 8), _pos(0) {}

  bool get(uint8_t bits, uint32_t& value) {
    if (_pos + bits > _bits) return false;
    uint32_t v = 0;
    for (uint8_t k = 0; k < bits; ++k) {
      uint8_t b = (uint8_t)((_src[_pos >> 3] >> (7 - (_pos & 7))) & 1);
      v = (v << 1) | b;
      ++_pos;
    }
    value = v;
    return true;
  }

private:
  const uint8_t* _src;
  size_t _bits;
  size_t _pos;
};

/// Writes bytes into a fixed buffer with overflow tracking.
class ByteWriter {
public:
  ByteWriter(uint8_t* dst, size_t cap) : _dst(dst), _cap(cap), _pos(0), _ok(true) {}

  void put(uint8_t b) {
    if (_pos < _cap) {
      _dst[_pos] = b;
    } else {
      _ok = false;
    }
    ++_pos;
  }

  void copy(const uint8_t* src, size_t n) {
    if (_pos + n <= _cap) {
      memcpy(_dst + _pos, src, n);
    } else {
      _ok = false;
    }
    _pos += n;
  }

  /// Number of bytes written, or 0 if the buffer overflowed.
  size_t size() const { return _ok ? _pos : 0; }

private:
  uint8_t* _dst;
  size_t _cap;
  size_t _pos;
  bool _ok;
};

}  // namespace detail
}  // namespace TinyCompression
