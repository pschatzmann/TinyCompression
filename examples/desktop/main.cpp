// Runs the RoundTrip Arduino sketch on the desktop, unchanged.
// Serial writes to stdout; setup() and loop() are called from main().
#include <tinycompression/desktop/Arduino.h>
#include <TinyCompression.h>

using namespace TinyCompression;  // the sketch relies on the namespace import done on Arduino

#include "../RoundTrip/RoundTrip.ino"

int main() {
  setup();
  loop();
  return 0;
}
