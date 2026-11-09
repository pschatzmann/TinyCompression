#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Platform.h"
#include "Codec.h"

namespace TinyCompression {

namespace detail {
// Frame header: [codec id (0 = stored)] [raw length LE16] [payload length LE16]
enum { kFrameHeaderSize = 5 };
enum { kMaxBlockSize = 32768 };

inline size_t clampBlockSize(size_t v) {
  if (v < 1) return 1;
  if (v > kMaxBlockSize) return kMaxBlockSize;
  return v;
}
}  // namespace detail

/// Print that compresses everything written to it and forwards the frames to
/// another Print. Data is compressed incrementally: a frame is emitted every time
/// a block of blockSize bytes is full (or on flush()). Nothing is buffered beyond one block.
class CompressionWriter : public Print {
public:
  CompressionWriter(Print& out, Codec& codec, size_t blockSize = 256)
    : _out(out),
      _codec(codec),
      _blockSize(detail::clampBlockSize(blockSize)),
      _raw(nullptr),
      _enc(nullptr),
      _fill(0),
      _bytesIn(0),
      _bytesOut(0),
      _error(false) {
    _raw = (uint8_t*)malloc(_blockSize);
    _enc = (uint8_t*)malloc(_blockSize);
  }

  ~CompressionWriter() {
    free(_raw);
    free(_enc);
  }

  CompressionWriter(const CompressionWriter&) = delete;
  CompressionWriter& operator=(const CompressionWriter&) = delete;

  using Print::write;

  size_t write(uint8_t b) override {
    if (!ok()) return 0;
    _raw[_fill++] = b;
    if (_fill == _blockSize && !emitBlock()) return 0;
    return 1;
  }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!ok()) return 0;
    size_t done = 0;
    while (done < size) {
      size_t room = _blockSize - _fill;
      size_t chunk = detail::minOf(room, size - done);
      memcpy(_raw + _fill, buf + done, chunk);
      _fill += chunk;
      done += chunk;
      if (_fill == _blockSize && !emitBlock()) break;
    }
    return done;
  }

  /// Emits the pending (partial) block and flushes the underlying Print.
  void flush() override {
    if (ok() && _fill) emitBlock();
    _out.flush();
  }

  /// True while memory is allocated and no write error has occurred.
  bool ok() const { return _raw && _enc && !_error; }
  size_t blockSize() const { return _blockSize; }
  size_t bytesIn() const { return _bytesIn; }   // uncompressed bytes accepted
  size_t bytesOut() const { return _bytesOut; } // bytes sent to the underlying Print

private:
  bool emitBlock() {
    size_t n = _fill;
    _fill = 0;
    if (n == 0) return true;

    // Only keep the compressed result if it is smaller than the raw block.
    size_t enc = _codec.encode(_raw, n, _enc, n - 1);
    uint8_t hdr[detail::kFrameHeaderSize];
    const uint8_t* payload;
    size_t len;
    if (enc > 0 && enc < n) {
      hdr[0] = _codec.id();
      payload = _enc;
      len = enc;
    } else {
      hdr[0] = 0;
      payload = _raw;
      len = n;
    }
    hdr[1] = (uint8_t)(n & 0xFF);
    hdr[2] = (uint8_t)(n >> 8);
    hdr[3] = (uint8_t)(len & 0xFF);
    hdr[4] = (uint8_t)(len >> 8);

    if (_out.write(hdr, detail::kFrameHeaderSize) != detail::kFrameHeaderSize ||
        _out.write(payload, len) != len) {
      _error = true;
      return false;
    }
    _bytesIn += n;
    _bytesOut += detail::kFrameHeaderSize + len;
    return true;
  }

  Print& _out;
  Codec& _codec;
  size_t _blockSize;
  uint8_t* _raw;
  uint8_t* _enc;
  size_t _fill;
  size_t _bytesIn;
  size_t _bytesOut;
  bool _error;
};

/// Stream that decompresses frames read from another Stream. Frames are decoded
/// on demand, one block at a time, so the input is consumed incrementally.
/// blockSize must be >= the blockSize used by the CompressionWriter.
class CompressionReader : public Stream {
public:
  CompressionReader(Stream& in, Codec& codec, size_t blockSize = 256)
    : _in(in),
      _codec(codec),
      _blockSize(detail::clampBlockSize(blockSize)),
      _raw(nullptr),
      _payload(nullptr),
      _pos(0),
      _len(0),
      _error(false) {
    _raw = (uint8_t*)malloc(_blockSize);
    _payload = (uint8_t*)malloc(_blockSize);
  }

  ~CompressionReader() {
    free(_raw);
    free(_payload);
  }

  CompressionReader(const CompressionReader&) = delete;
  CompressionReader& operator=(const CompressionReader&) = delete;

  using Stream::readBytes;
  using Print::write;

  /// Bytes already decoded and waiting to be read. If the buffer is empty, the
  /// next frame is decoded when the input holds a complete header.
  int available() override {
    if (_pos >= _len) fillBlock();
    return (int)(_len - _pos);
  }

  int read() override {
    if (_pos >= _len && !fillBlock()) return -1;
    return _raw[_pos++];
  }

  int peek() override {
    if (_pos >= _len && !fillBlock()) return -1;
    return _raw[_pos];
  }

  // Not marked override: the AVR core's Stream::readBytes is not virtual.
  size_t readBytes(char* buffer, size_t length) {
    size_t got = 0;
    while (got < length) {
      if (_pos >= _len && !fillBlock()) break;
      size_t n = detail::minOf(length - got, _len - _pos);
      memcpy(buffer + got, _raw + _pos, n);
      _pos += n;
      got += n;
    }
    return got;
  }

  /// The reader is read-only.
  size_t write(uint8_t) override { return 0; }

  /// True while memory is allocated and no decoding error has occurred.
  bool ok() const { return _raw && _payload && !_error; }
  bool error() const { return _error; }
  size_t blockSize() const { return _blockSize; }

private:
  bool fillBlock() {
    _pos = 0;
    _len = 0;
    if (!ok()) return false;
    if (_in.available() < detail::kFrameHeaderSize) return false;

    uint8_t hdr[detail::kFrameHeaderSize];
    if (_in.readBytes((char*)hdr, detail::kFrameHeaderSize) != detail::kFrameHeaderSize) {
      _error = true;
      return false;
    }
    uint8_t id = hdr[0];
    size_t rawLen = hdr[1] | (hdr[2] << 8);
    size_t encLen = hdr[3] | (hdr[4] << 8);

    if (rawLen == 0 || rawLen > _blockSize || encLen > rawLen) return fail();
    if (id == 0 && encLen != rawLen) return fail();
    if (id != 0 && (id != _codec.id() || encLen == 0)) return fail();

    if (encLen && _in.readBytes((char*)_payload, encLen) != encLen) return fail();

    if (id == 0) {
      memcpy(_raw, _payload, rawLen);
    } else if (!_codec.decode(_payload, encLen, _raw, rawLen)) {
      return fail();
    }
    _len = rawLen;
    return true;
  }

  bool fail() {
    _error = true;
    _len = 0;
    _pos = 0;
    return false;
  }

  Stream& _in;
  Codec& _codec;
  size_t _blockSize;
  uint8_t* _raw;
  uint8_t* _payload;
  size_t _pos;
  size_t _len;
  bool _error;
};

}  // namespace TinyCompression
