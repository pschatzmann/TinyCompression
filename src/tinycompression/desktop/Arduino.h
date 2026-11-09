#pragma once
// Desktop replacements for the Arduino globals used by sketches: Serial and millis().
// Serial writes to stdout. Include this (and use setup()/loop() from a main()) to run
// an Arduino sketch on the desktop; see examples/desktop/main.cpp.
#include <chrono>
#include <stdio.h>
#include "Print.h"
#include "Stream.h"

class DesktopSerial : public Print {
public:
  using Print::write;

  void begin(unsigned long) {}
  operator bool() const { return true; }

  size_t write(uint8_t b) override {
    return fputc(b, stdout) == EOF ? 0 : 1;
  }
  void flush() override { fflush(stdout); }
};

static DesktopSerial Serial;

inline unsigned long millis() {
  static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start)
      .count();
}
