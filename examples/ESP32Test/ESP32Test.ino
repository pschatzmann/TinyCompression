// Hardware test for TinyCompression on ESP32.
//
// Round-trips deterministic data through every codec, several block sizes and
// input sizes, checks that corrupted and truncated streams are rejected, and
// prints a benchmark. Results are printed as lines starting with PASS / FAIL,
// and the last line is "SUMMARY passed=N failed=M" so a host script can check it.
#include <TinyCompression.h>
#include <vector>

using namespace TinyCompression;

static int passed = 0;
static int failed = 0;

static void record(bool ok, const char* name) {
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.println(name);
  if (ok) {
    passed++;
  } else {
    failed++;
  }
}

// ---- deterministic test data ------------------------------------------------

enum DataKind { kZeros, kRamp, kText, kRandom, kRuns, kDataKinds };

static const char* kindName(int kind) {
  switch (kind) {
    case kZeros: return "zeros";
    case kRamp: return "ramp";
    case kText: return "text";
    case kRandom: return "random";
    case kRuns: return "runs";
  }
  return "?";
}

static uint32_t xorshift(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

static std::vector<uint8_t> makeData(int kind, size_t n, uint32_t seed = 12345) {
  std::vector<uint8_t> v(n);
  uint32_t s = seed ? seed : 1;
  static const char* words[] = {"sensor", "value", "temp", "42", "ok", "tiny", "stream", "data "};
  switch (kind) {
    case kZeros:
      break;
    case kRamp:
      for (size_t i = 0; i < n; ++i) v[i] = (uint8_t)i;
      break;
    case kText: {
      size_t i = 0;
      while (i < n) {
        const char* w = words[xorshift(s) % 8];
        for (const char* c = w; *c && i < n; ++c) v[i++] = (uint8_t)*c;
        if (i < n) v[i++] = ' ';
      }
      break;
    }
    case kRandom:
      for (size_t i = 0; i < n; ++i) v[i] = (uint8_t)xorshift(s);
      break;
    case kRuns: {
      size_t i = 0;
      while (i < n) {
        uint8_t val = (uint8_t)xorshift(s);
        size_t len = 1 + (xorshift(s) % 40);
        for (size_t k = 0; k < len && i < n; ++k) v[i++] = val;
      }
      break;
    }
  }
  return v;
}

// ---- codecs under test ------------------------------------------------------

RleCodec rle;
DeltaCodec delta;
LzssCodec lzss;
HeatshrinkCodec hsDefault(8, 4);
HeatshrinkCodec hsSmall(4, 2);
HeatshrinkCodec hsLarge(12, 5);
Lz4Codec lz4;

static Codec* const kCodecs[] = {&rle, &delta, &lzss, &hsDefault, &hsSmall, &hsLarge, &lz4};
static const size_t kNumCodecs = sizeof(kCodecs) / sizeof(kCodecs[0]);

// Writes `in` through a CompressionWriter into `ram`, then reads it back.
// Returns true when the output equals the input and no error was reported.
static bool roundTrip(Codec& codec, const std::vector<uint8_t>& in, size_t block,
                      RamStream& ram, bool& writerOk, bool& readerErr) {
  ram.clear();
  {
    CompressionWriter w(ram, codec, block);
    w.write(in.data(), in.size());
    w.flush();
    writerOk = w.ok() && w.bytesIn() == in.size();
  }
  ram.rpos = 0;
  CompressionReader r(ram, codec, block);
  std::vector<uint8_t> out;
  out.reserve(in.size());
  while (r.available()) out.push_back((uint8_t)r.read());
  readerErr = r.error();
  return writerOk && !readerErr && out == in;
}

// ---- test groups ------------------------------------------------------------

static void testRoundTrips() {
  const size_t sizes[] = {0, 1, 100, 4096};
  const size_t blocks[] = {16, 256};
  RamStream ram;

  for (size_t c = 0; c < kNumCodecs; c++) {
    Codec& codec = *kCodecs[c];
    for (int kind = 0; kind < kDataKinds; kind++) {
      for (size_t b = 0; b < sizeof(blocks) / sizeof(blocks[0]); b++) {
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
          std::vector<uint8_t> in = makeData(kind, sizes[s]);
          bool wOk = false, rErr = false;
          bool ok = roundTrip(codec, in, blocks[b], ram, wOk, rErr);
          char name[80];
          snprintf(name, sizeof(name), "roundtrip %s %s n=%u block=%u", codec.name(),
                   kindName(kind), (unsigned)sizes[s], (unsigned)blocks[b]);
          record(ok, name);
        }
      }
    }
  }
}

static void testCorruptionDetected() {
  const std::vector<uint8_t> in = makeData(kText, 1024);
  RamStream ram;
  bool wOk = false, rErr = false;

  // Unknown codec id in the first frame header.
  {
    HeatshrinkCodec codec(8, 4);
    ram.clear();
    CompressionWriter w(ram, codec, 256);
    w.write(in.data(), in.size());
    w.flush();
    ram.data[0] = 0xEE;
    ram.rpos = 0;
    CompressionReader r(ram, codec, 256);
    while (r.available()) r.read();
    record(r.error(), "corrupt: unknown codec id is rejected");
  }

  // Stream cut short in the middle of the last frame.
  {
    LzssCodec codec;
    ram.clear();
    CompressionWriter w(ram, codec, 256);
    w.write(in.data(), in.size());
    w.flush();
    ram.data.pop_back();
    ram.rpos = 0;
    CompressionReader r(ram, codec, 256);
    while (r.available()) r.read();
    record(r.error(), "corrupt: truncated frame is rejected");
  }

  // Reader block size smaller than the writer's block size.
  {
    RleCodec codec;
    ram.clear();
    CompressionWriter w(ram, codec, 256);
    w.write(in.data(), in.size());
    w.flush();
    ram.rpos = 0;
    CompressionReader r(ram, codec, 16);
    while (r.available()) r.read();
    record(r.error(), "corrupt: reader block too small is rejected");
  }

  (void)wOk;
  (void)rErr;
}

static void testCompressionRatio() {
  // Highly repetitive input must shrink; random input must not grow beyond framing overhead.
  RamStream ram;
  bool wOk = false, rErr = false;

  std::vector<uint8_t> zeros = makeData(kZeros, 4096);
  Lz4Codec lz;
  roundTrip(lz, zeros, 256, ram, wOk, rErr);
  record(ram.data.size() < zeros.size() / 4, "ratio: LZ4 compresses 4 KB of zeros below 25%");

  std::vector<uint8_t> random = makeData(kRandom, 4096);
  roundTrip(lz, random, 256, ram, wOk, rErr);
  // 5-byte header per 256-byte block: 16 blocks -> 80 bytes of overhead at most.
  record(ram.data.size() <= random.size() + 16 * 5, "ratio: random data stored with framing overhead only");
}

static void benchmark() {
  const size_t n = 8192;
  std::vector<uint8_t> in = makeData(kText, n);
  Serial.println("BENCH codec input_bytes compressed_bytes ratio enc_ms dec_ms");
  for (size_t c = 0; c < kNumCodecs; c++) {
    Codec& codec = *kCodecs[c];
    RamStream ram;
    unsigned long t0 = micros();
    {
      CompressionWriter w(ram, codec, 256);
      w.write(in.data(), in.size());
      w.flush();
    }
    unsigned long t1 = micros();
    ram.rpos = 0;
    CompressionReader r(ram, codec, 256);
    size_t got = 0;
    while (r.available()) {
      r.read();
      got++;
    }
    unsigned long t2 = micros();
    Serial.printf("BENCH %s %u %u %.2f %.2f %.2f\n", codec.name(), (unsigned)n,
                  (unsigned)ram.data.size(), (double)ram.data.size() * 100.0 / n,
                  (t1 - t0) / 1000.0, (t2 - t1) / 1000.0);
    (void)got;
  }
}

// ---- entry point ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("=== TinyCompression ESP32 test ===");
  Serial.printf("Chip: %s  free heap at start: %u bytes\n", ESP.getChipModel(),
                (unsigned)ESP.getFreeHeap());

  uint32_t heapStart = ESP.getFreeHeap();

  testRoundTrips();
  testCorruptionDetected();
  testCompressionRatio();
  benchmark();

  int32_t heapDelta = (int32_t)ESP.getFreeHeap() - (int32_t)heapStart;
  Serial.printf("INFO heap delta after tests: %d bytes\n", (int)heapDelta);
  Serial.printf("SUMMARY passed=%d failed=%d\n", passed, failed);
  Serial.println("DONE");
}

void loop() {}
