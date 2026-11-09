#include <vector>
#include "TestFramework.h"
#include "TestData.h"
#include "TinyCompression.h"

using namespace TinyCompression;
using namespace tc_test;

namespace {

struct AllCodecs {
  RleCodec rle;
  DeltaCodec delta;
  LzssCodec lzss;
  HeatshrinkCodec hs;
  HeatshrinkCodec hsSmall{4, 2};
  HeatshrinkCodec hsLarge{12, 5};
  Lz4Codec lz4;

  std::vector<Codec*> list() {
    return {&rle, &delta, &lzss, &hs, &hsSmall, &hsLarge, &lz4};
  }
};

// Writes `in` through a CompressionWriter in uneven chunks, returns the compressed bytes.
std::vector<uint8_t> compress(Codec& c, const std::vector<uint8_t>& in, size_t blockSize) {
  RamStream channel;
  CompressionWriter w(channel, c, blockSize);
  size_t pos = 0;
  size_t chunk = 1;
  while (pos < in.size()) {
    size_t n = in.size() - pos < chunk ? in.size() - pos : chunk;
    if (n == 1) {
      w.write(in[pos]);
    } else {
      w.write(in.data() + pos, n);
    }
    pos += n;
    chunk = chunk % 37 + 1;
  }
  w.flush();
  CHECK(w.ok());
  CHECK(w.bytesIn() == in.size());
  CHECK(w.bytesOut() == channel.data.size());
  return channel.data;
}

std::vector<uint8_t> decompressAll(Codec& c, RamStream& channel, size_t blockSize) {
  CompressionReader r(channel, c, blockSize);
  std::vector<uint8_t> out;
  int b;
  while ((b = r.read()) >= 0) out.push_back((uint8_t)b);
  CHECK(!r.error());
  return out;
}

}  // namespace

TEST_CASE(stream_roundtrip_all_codecs_and_block_sizes) {
  AllCodecs all;
  const size_t sizes[] = {0, 1, 5, 13, 255, 256, 257, 1000, 4100};
  const size_t blocks[] = {1, 7, 64, 256, 1000};
  for (Codec* c : all.list()) {
    for (size_t n : sizes) {
      for (size_t bs : blocks) {
        std::vector<uint8_t> in = makeData(kText, n, (uint32_t)(n + bs));
        std::vector<uint8_t> enc = compress(*c, in, bs);
        RamStream channel;
        channel.data = enc;
        std::vector<uint8_t> out = decompressAll(*c, channel, bs);
        CHECK(out == in);
      }
    }
  }
}

TEST_CASE(stream_incremental_read_and_readbytes) {
  // Data arrives in pieces: the reader must hand out bytes before the writer is finished.
  Lz4Codec lz4;
  RamStream channel;
  CompressionWriter w(channel, lz4, 64);
  std::vector<uint8_t> in = makeData(kText, 300, 3);
  w.write(in.data(), 100);
  // Two full blocks would be emitted for 128 bytes; 100 bytes gives one frame already.
  CHECK(channel.data.size() > 0);
  size_t framesSoFar = channel.data.size();
  CompressionReader r(channel, lz4, 64);
  CHECK(r.available() == 64);
  CHECK(r.read() == in[0]);
  w.write(in.data() + 100, 200);
  w.flush();
  CHECK(channel.data.size() > framesSoFar);

  std::vector<uint8_t> got;
  char buf[17];
  while (true) {
    size_t n = r.readBytes(buf, sizeof(buf));
    for (size_t i = 0; i < n; ++i) got.push_back((uint8_t)buf[i]);
    if (n < sizeof(buf)) break;
  }
  got.insert(got.begin(), in[0]);
  CHECK(got == in);
  CHECK(!r.error());
}

TEST_CASE(stream_flush_emits_partial_block) {
  RleCodec rle;
  RamStream channel;
  CompressionWriter w(channel, rle, 256);
  w.print("abc");
  CHECK(channel.data.empty());  // nothing emitted before the block is full or flushed
  w.flush();
  CHECK(!channel.data.empty());
  CompressionReader r(channel, rle, 256);
  CHECK(r.read() == 'a');
  CHECK(r.read() == 'b');
  CHECK(r.read() == 'c');
  CHECK(r.read() == -1);
  CHECK(!r.error());
}

TEST_CASE(stream_empty_input) {
  RleCodec rle;
  RamStream channel;
  CompressionWriter w(channel, rle);
  w.flush();
  CHECK(channel.data.empty());
  CompressionReader r(channel, rle);
  CHECK(r.available() == 0);
  CHECK(r.read() == -1);
  CHECK(!r.error());
}

TEST_CASE(stream_stored_fallback_bounds_overhead) {
  // Incompressible data must cost at most one 5-byte header per block.
  Lz4Codec lz4;
  std::vector<uint8_t> in = makeData(kRandom, 1000, 77);
  std::vector<uint8_t> enc = compress(lz4, in, 256);
  CHECK(enc.size() <= in.size() + 5 * 4);
  RamStream channel;
  channel.data = enc;
  CHECK(decompressAll(lz4, channel, 256) == in);
}

TEST_CASE(stream_compresses_runs) {
  RleCodec rle;
  std::vector<uint8_t> in = makeData(kZeros, 10000);
  std::vector<uint8_t> enc = compress(rle, in, 256);
  CHECK(enc.size() < 1000);
}

TEST_CASE(stream_codec_mismatch_is_detected) {
  RleCodec rle;
  Lz4Codec lz4;
  // Zeros compress with RLE, so the frames carry RLE's codec id (stored frames would not).
  std::vector<uint8_t> in = makeData(kZeros, 300);
  std::vector<uint8_t> enc = compress(rle, in, 128);
  CHECK(enc[0] == rle.id());
  RamStream channel;
  channel.data = enc;
  CompressionReader r(channel, lz4, 128);
  CHECK(r.read() == -1);
  CHECK(r.error());
}

TEST_CASE(stream_truncated_input_is_detected) {
  Lz4Codec lz4;
  std::vector<uint8_t> in = makeData(kText, 300, 11);
  std::vector<uint8_t> enc = compress(lz4, in, 128);
  enc.pop_back();
  RamStream channel;
  channel.data = enc;
  CompressionReader r(channel, lz4, 128);
  size_t count = 0;
  while (r.read() >= 0) ++count;
  CHECK(count < in.size());
  CHECK(r.error());
}

TEST_CASE(stream_reader_block_size_too_small_is_error) {
  RleCodec rle;
  std::vector<uint8_t> in = makeData(kZeros, 300);
  std::vector<uint8_t> enc = compress(rle, in, 256);
  RamStream channel;
  channel.data = enc;
  CompressionReader r(channel, rle, 64);
  CHECK(r.read() == -1);
  CHECK(r.error());
}

TEST_CASE(stream_heatshrink_parameters_are_clamped) {
  HeatshrinkCodec hs(20, 30);
  CHECK(hs.windowBits() == 13);
  CHECK(hs.lookaheadBits() == 12);
  std::vector<uint8_t> in = makeData(kRuns, 2000, 4);
  std::vector<uint8_t> enc = compress(hs, in, 512);
  RamStream channel;
  channel.data = enc;
  CHECK(decompressAll(hs, channel, 512) == in);
}
