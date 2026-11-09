#pragma once
// RAM-backed Stream: writes append to a std::vector, reads consume from it.
// Not available on AVR boards (Uno, Nano, ...), which have no std::vector.
#include "Platform.h"

#if !defined(ARDUINO_ARCH_AVR)
#include <vector>

namespace TinyCompression {

class RamStream : public Stream {
public:
  using Print::write;

  size_t write(uint8_t b) override {
    data.push_back(b);
    return 1;
  }
  int available() override { return (int)(data.size() - rpos); }
  int read() override { return rpos < data.size() ? data[rpos++] : -1; }
  int peek() override { return rpos < data.size() ? data[rpos] : -1; }

  /// Removes all data and rewinds the read position.
  void clear() {
    data.clear();
    rpos = 0;
  }

  std::vector<uint8_t> data;  // stored bytes
  size_t rpos = 0;            // next byte to read
};

}  // namespace TinyCompression

#endif  // !ARDUINO_ARCH_AVR
