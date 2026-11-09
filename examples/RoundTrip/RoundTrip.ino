// Compresses text into a RAM buffer with CompressionWriter, then reads it back
// with CompressionReader. Swap HeatshrinkCodec for Lz4Codec on boards with more RAM.
#include <TinyCompression.h>

#if defined(ARDUINO_ARCH_AVR)
// AVR boards have no std::vector, so the example uses a fixed-size buffer.
class RamStream : public Stream {
public:
  using Print::write;

  size_t write(uint8_t b) override {
    if (len >= sizeof(data)) return 0;
    data[len++] = b;
    return 1;
  }
  int available() override { return (int)(len - rpos); }
  int read() override { return rpos < len ? data[rpos++] : -1; }
  int peek() override { return rpos < len ? data[rpos] : -1; }

  uint8_t data[256];
  size_t len = 0;
  size_t rpos = 0;
};
#else
using RamStream = TinyCompression::RamStream;
#endif

RamStream ram;
HeatshrinkCodec codec(8, 4);  // window 256, lookahead 16

const char text[] =
    "TinyCompression compresses TinyCompression streams. "
    "TinyCompression compresses TinyCompression streams. ";

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

  // Compress: data is emitted in blocks of 128 bytes as it is written.
  {
    CompressionWriter writer(ram, codec, 128);
    writer.print(text);
    writer.flush();
    Serial.print("input bytes: ");
    Serial.print(writer.bytesIn());
    Serial.print("  compressed bytes: ");
    Serial.println(writer.bytesOut());
  }

  // Decompress: frames are decoded on demand while reading.
  ram.rpos = 0;
  CompressionReader reader(ram, codec, 128);
  Serial.println("decoded:");
  while (reader.available()) {
    Serial.write((char)reader.read());
  }
  Serial.println();
  Serial.println(reader.error() ? "ERROR" : "OK");
}

void loop() {}
