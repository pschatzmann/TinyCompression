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

bool roundTrip(Codec& c, const std::vector<uint8_t>& in) {
  std::vector<uint8_t> enc(in.size() * 2 + 64);
  size_t e = c.encode(in.data(), in.size(), enc.data(), enc.size());
  if (e == 0) return false;
  std::vector<uint8_t> out(in.size());
  if (!c.decode(enc.data(), e, out.data(), out.size())) return false;
  return out == in;
}

}  // namespace

TEST_CASE(rle_known_vector) {
  // "AAAB" -> run of 3 'A' (header 128+1), literal 'B' (header 0)
  const uint8_t in[] = {'A', 'A', 'A', 'B'};
  const uint8_t expected[] = {129, 'A', 0, 'B'};
  RleCodec c;
  uint8_t enc[16];
  size_t e = c.encode(in, 4, enc, sizeof(enc));
  CHECK(e == 4);
  CHECK(memcmp(enc, expected, 4) == 0);
  uint8_t dec[4];
  CHECK(c.decode(expected, 4, dec, 4));
  CHECK(memcmp(dec, in, 4) == 0);
}

TEST_CASE(delta_known_vector) {
  // diffs 1,1,1 -> 4-bit codes 8,8,8 -> bits 1000 1000 1000 + padding
  const uint8_t in[] = {1, 2, 3};
  const uint8_t expected[] = {0x88, 0x80};
  DeltaCodec c;
  uint8_t enc[8];
  size_t e = c.encode(in, 3, enc, sizeof(enc));
  CHECK(e == 2);
  CHECK(memcmp(enc, expected, 2) == 0);
}

TEST_CASE(lz4_known_literals) {
  // Block shorter than 13 bytes: a single literal-only sequence.
  const uint8_t in[] = {'a', 'b', 'c'};
  const uint8_t expected[] = {0x30, 'a', 'b', 'c'};
  Lz4Codec c;
  uint8_t enc[8];
  size_t e = c.encode(in, 3, enc, sizeof(enc));
  CHECK(e == 4);
  CHECK(memcmp(enc, expected, 4) == 0);
}

TEST_CASE(lz4_known_match) {
  // "abcd" literal + match(offset 4, length 7) + final literals "dabcd"
  const uint8_t in[] = "abcdabcdabcdabcd";
  const uint8_t expected[] = {0x43, 'a', 'b', 'c', 'd', 0x04, 0x00, 0x50, 'd', 'a', 'b', 'c', 'd'};
  Lz4Codec c;
  uint8_t enc[32];
  size_t e = c.encode(in, 16, enc, sizeof(enc));
  CHECK(e == sizeof(expected));
  CHECK(memcmp(enc, expected, sizeof(expected)) == 0);
  uint8_t dec[16];
  CHECK(c.decode(expected, sizeof(expected), dec, 16));
  CHECK(memcmp(dec, in, 16) == 0);
}

TEST_CASE(codec_roundtrip_matrix) {
  AllCodecs all;
  const size_t sizes[] = {1, 2, 3, 4, 12, 13, 16, 17, 100, 255, 256, 257, 1000, 4096};
  for (Codec* c : all.list()) {
    for (int kind = 0; kind < kDataKinds; ++kind) {
      for (size_t n : sizes) {
        std::vector<uint8_t> data = makeData(kind, n, (uint32_t)(n * 7 + kind + 1));
        CHECK(roundTrip(*c, data));
      }
    }
  }
}

TEST_CASE(codecs_compress_their_target_data) {
  AllCodecs all;
  std::vector<uint8_t> zeros = makeData(kZeros, 256);
  std::vector<uint8_t> ramp = makeData(kRamp, 256);
  std::vector<uint8_t> text = makeData(kText, 1024);
  uint8_t buf[2048];

  CHECK(all.rle.encode(zeros.data(), zeros.size(), buf, sizeof(buf)) < 32);
  CHECK(all.delta.encode(ramp.data(), ramp.size(), buf, sizeof(buf)) < 200);
  CHECK(all.lz4.encode(text.data(), text.size(), buf, sizeof(buf)) < 600);
  CHECK(all.lzss.encode(text.data(), text.size(), buf, sizeof(buf)) < 800);
  CHECK(all.hs.encode(text.data(), text.size(), buf, sizeof(buf)) < 800);
}

TEST_CASE(codec_encode_respects_capacity) {
  AllCodecs all;
  std::vector<uint8_t> random = makeData(kRandom, 100);
  for (Codec* c : all.list()) {
    uint8_t buf[10];
    CHECK(c->encode(random.data(), random.size(), buf, sizeof(buf)) == 0);
    CHECK(c->encode(random.data(), random.size(), buf, 0) == 0);
  }
}

TEST_CASE(decode_rejects_wrong_length) {
  // Delta is excluded: its padding bits can form extra samples, so the frame
  // header (exact length) is what guarantees correctness there.
  AllCodecs all;
  std::vector<uint8_t> zeros = makeData(kZeros, 100);
  Codec* codecs[] = {&all.rle, &all.lzss, &all.hs, &all.lz4};
  for (Codec* c : codecs) {
    std::vector<uint8_t> enc(256);
    size_t e = c->encode(zeros.data(), zeros.size(), enc.data(), enc.size());
    CHECK(e > 0);
    std::vector<uint8_t> out(101);
    CHECK(!c->decode(enc.data(), e, out.data(), out.size()));
  }
}

TEST_CASE(decode_garbage_is_safe) {
  // Run under AddressSanitizer/UBSan (TC_SANITIZE=ON) to catch out-of-bounds access.
  AllCodecs all;
  uint32_t s = 99;
  for (int iter = 0; iter < 2000; ++iter) {
    size_t n = xorshift(s) % 64;
    std::vector<uint8_t> junk(n);
    for (auto& b : junk) b = (uint8_t)xorshift(s);
    size_t dstLen = 1 + xorshift(s) % 300;
    std::vector<uint8_t> out(dstLen);
    for (Codec* c : all.list()) {
      c->decode(junk.data(), junk.size(), out.data(), out.size());
    }
  }
  // Truncated and bit-flipped valid streams must not crash either.
  std::vector<uint8_t> text = makeData(kText, 512, 5);
  for (Codec* c : all.list()) {
    std::vector<uint8_t> enc(1024);
    size_t e = c->encode(text.data(), text.size(), enc.data(), enc.size());
    CHECK(e > 0);
    std::vector<uint8_t> out(text.size());
    for (size_t cut = 0; cut < e; ++cut) c->decode(enc.data(), cut, out.data(), out.size());
    for (size_t i = 0; i < e; ++i) {
      uint8_t mask = (uint8_t)(1u << (xorshift(s) % 8));
      enc[i] ^= mask;
      c->decode(enc.data(), e, out.data(), out.size());
      enc[i] ^= mask;  // restore
    }
  }
}
