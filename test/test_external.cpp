// Decodes data produced by external tools (reference lz4 CLI) with the library.
// Fixtures live in test/fixtures and are created by generate_lz4_fixtures.py.
#include <cstdio>
#include <string>
#include <vector>
#include "TestFramework.h"
#include "TinyCompression.h"

#ifndef TC_FIXTURE_DIR
#error "TC_FIXTURE_DIR must point to test/fixtures"
#endif

using namespace TinyCompression;
using namespace tc_test;

namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  int c;
  while ((c = std::fgetc(f)) != EOF) out.push_back((uint8_t)c);
  std::fclose(f);
  return true;
}

std::string fixture(const char* file) {
  return std::string(TC_FIXTURE_DIR) + "/" + file;
}

// Decodes <name>.<level>.lz4 with Lz4Codec and compares against <name>.raw.
void checkLz4Fixture(const char* name, int level) {
  std::vector<uint8_t> enc, raw;
  char encFile[128];
  std::snprintf(encFile, sizeof(encFile), "%s.l%d.lz4", name, level);
  CHECK(readFile(fixture(encFile), enc));
  CHECK(readFile(fixture((std::string(name) + ".raw").c_str()), raw));
  if (enc.empty() || raw.empty()) return;

  Lz4Codec lz4;
  std::vector<uint8_t> out(raw.size());
  bool ok = lz4.decode(enc.data(), enc.size(), out.data(), out.size());
  if (!ok) std::fprintf(stderr, "  decode failed: %s level %d\n", name, level);
  CHECK(ok);
  CHECK(out == raw);
}

}  // namespace

TEST_CASE(external_lz4_text) {
  checkLz4Fixture("text", 1);
  checkLz4Fixture("text", 9);
}

TEST_CASE(external_lz4_runs) {
  checkLz4Fixture("runs", 1);
  checkLz4Fixture("runs", 9);
}

TEST_CASE(external_lz4_ramp) {
  checkLz4Fixture("ramp", 1);
  checkLz4Fixture("ramp", 9);
}

TEST_CASE(external_lz4_long_match_extension) {
  // 35000 zero bytes: match lengths need many 255-valued extension bytes.
  checkLz4Fixture("zeros_long", 1);
  checkLz4Fixture("zeros_long", 9);
}

TEST_CASE(external_lz4_long_literal_extension) {
  // 600 random literals before the match: literal length extension bytes.
  checkLz4Fixture("literal_then_match", 1);
  checkLz4Fixture("literal_then_match", 9);
}

TEST_CASE(external_lz4_end_of_block_rules) {
  checkLz4Fixture("abcd_repeat", 1);
  checkLz4Fixture("abcd_repeat", 9);
}

// Decodes heatshrink_<name>.w<W>l<L>.hs (made by the reference heatshrink library)
// with HeatshrinkCodec(W, L) and compares against heatshrink_<name>.raw.
void checkHeatshrinkFixture(const char* name, uint8_t w, uint8_t l) {
  char encFile[128];
  std::snprintf(encFile, sizeof(encFile), "heatshrink_%s.w%dl%d.hs", name, (int)w, (int)l);
  std::vector<uint8_t> enc, raw;
  CHECK(readFile(fixture(encFile), enc));
  CHECK(readFile(fixture((std::string("heatshrink_") + name + ".raw").c_str()), raw));
  if (enc.empty() || raw.empty()) return;

  HeatshrinkCodec hs(w, l);
  CHECK(hs.windowBits() == w && hs.lookaheadBits() == l);  // parameters must not be clamped
  std::vector<uint8_t> out(raw.size());
  bool ok = hs.decode(enc.data(), enc.size(), out.data(), out.size());
  if (!ok) std::fprintf(stderr, "  decode failed: %s w=%d l=%d\n", name, (int)w, (int)l);
  CHECK(ok);
  CHECK(out == raw);
}

TEST_CASE(external_heatshrink_text) {
  checkHeatshrinkFixture("text", 4, 3);
  checkHeatshrinkFixture("text", 8, 4);
  checkHeatshrinkFixture("text", 10, 6);
  checkHeatshrinkFixture("text", 12, 5);
  checkHeatshrinkFixture("text", 13, 7);
}

TEST_CASE(external_heatshrink_runs_random_ramp) {
  checkHeatshrinkFixture("runs", 8, 4);
  checkHeatshrinkFixture("random", 8, 4);
  checkHeatshrinkFixture("ramp", 4, 3);
  checkHeatshrinkFixture("ramp", 10, 6);
  checkHeatshrinkFixture("ramp", 13, 7);
}

TEST_CASE(external_lz4_corrupted_reference_block_is_rejected) {
  // Flipping the offset of the first match in a reference block must not decode to the original.
  std::vector<uint8_t> enc, raw;
  CHECK(readFile(fixture("abcd_repeat.l9.lz4"), enc));
  CHECK(readFile(fixture("abcd_repeat.raw"), raw));
  if (enc.size() < 6) return;
  enc[enc.size() > 6 ? 5 : 1] ^= 0x01;  // corrupt the byte after the first literal run
  Lz4Codec lz4;
  std::vector<uint8_t> out(raw.size());
  bool ok = lz4.decode(enc.data(), enc.size(), out.data(), out.size());
  CHECK(!ok || out != raw);
}
