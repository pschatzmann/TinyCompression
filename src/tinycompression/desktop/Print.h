#pragma once
// Desktop stand-in for the Arduino Print class (subset of the Arduino API).
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

  virtual void flush() {}

  size_t print(const char* str) { return write(str); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { return printNumber("%d", v); }
  size_t print(unsigned int v) { return printNumber("%u", v); }
  size_t print(long v) { return printNumber("%ld", v); }
  size_t print(unsigned long v) { return printNumber("%lu", v); }

  size_t println() { return write((const uint8_t*)"\n", 1); }
  size_t println(const char* str) { return print(str) + println(); }
  size_t println(char c) { return print(c) + println(); }
  size_t println(int v) { return print(v) + println(); }
  size_t println(unsigned int v) { return print(v) + println(); }
  size_t println(long v) { return print(v) + println(); }
  size_t println(unsigned long v) { return print(v) + println(); }

private:
  template <typename T>
  size_t printNumber(const char* format, T v) {
    char buf[24];
    int n = snprintf(buf, sizeof(buf), format, v);
    return n > 0 ? write((const uint8_t*)buf, (size_t)n) : 0;
  }
};
