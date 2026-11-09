#pragma once
// Platform glue: uses the Arduino core's Print/Stream when building for a board,
// and a small desktop stand-in with the same API subset otherwise.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    size_t n = 0;
    while (n < size && write(buf[n]) == 1) ++n;
    return n;
  }
  size_t write(const char* str) {
    return str ? write((const uint8_t*)str, strlen(str)) : 0;
  }
  size_t print(const char* str) { return write(str); }
  virtual void flush() {}
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual size_t readBytes(char* buffer, size_t length) {
    size_t n = 0;
    while (n < length) {
      int c = read();
      if (c < 0) break;
      buffer[n++] = (char)c;
    }
    return n;
  }
  void setTimeout(unsigned long) {}
};
#endif
