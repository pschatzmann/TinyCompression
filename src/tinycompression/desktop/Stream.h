#pragma once
// Desktop stand-in for the Arduino Stream class (subset of the Arduino API).
#include "Print.h"

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  // Non-blocking on the desktop: returns what is available right now.
  virtual size_t readBytes(char* buffer, size_t length) {
    size_t n = 0;
    while (n < length) {
      int c = read();
      if (c < 0) break;
      buffer[n++] = (char)c;
    }
    return n;
  }

  size_t readBytes(uint8_t* buffer, size_t length) {
    return readBytes((char*)buffer, length);
  }

  void setTimeout(unsigned long) {}
};
