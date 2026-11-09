# Testing

The library is header-only, so there is nothing to build for the library itself. The desktop test suite compiles it with a standard C++11 compiler.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/tinycompression_tests
```

Or with CTest:

```bash
cd build && ctest --output-on-failure
```

To disable the tests, pass `-DTC_BUILD_TESTS=OFF`.

## Using the library on the desktop

The library builds on the desktop without the Arduino core. When `ARDUINO` is not defined, or when `TC_DESKTOP` is defined (the CMake interface target sets it for every consumer), `Platform.h` uses the stand-ins in `src/tinycompression/desktop/`:

- `Print.h` and `Stream.h`: the `Print` and `Stream` subset that the library uses, plus the common `print`/`println` overloads.
- `Arduino.h`: `Serial` (writes to stdout) and `millis()`, so Arduino sketches can run on the desktop.

`examples/desktop/main.cpp` includes the `RoundTrip` sketch unchanged and calls `setup()` and `loop()`. CMake builds it as `tinycompression_roundtrip` and checks its output with CTest. To use the library in a plain C++ program, add `src` to the include path and do not define `ARDUINO`.

## Sanitizers

```bash
cmake -S . -B build-asan -DTC_SANITIZE=ON
cmake --build build-asan
./build-asan/tinycompression_tests
```

This builds the tests with AddressSanitizer and UndefinedBehaviorSanitizer. The garbage-input test relies on it to catch out-of-bounds accesses in the decoders.

## What is tested

- **Known vectors**: RLE, Delta and LZ4 outputs for fixed inputs.
- **Round trips**: every codec on zeros, ramps, text, random data, runs and all byte values, at sizes around the block and token boundaries.
- **Capacity**: encoders return 0 instead of overflowing a too-small buffer.
- **Robustness**: random, truncated and bit-flipped input never crashes a decoder.
- **Streams**: writer/reader round trips across block sizes, incremental reads, `readBytes()`, `flush()` emitting partial blocks, stored fallback overhead, compression of runs, codec mismatch, truncated input, and a reader block size that is too small.

## Interoperability

The fixtures in `test/fixtures/` are produced by external tools, and the suite decodes them with the library (`test_external.cpp`):

| Codec | Source of fixtures | Regenerate with |
|---|---|---|
| `Lz4Codec` | reference `lz4` CLI v1.9+ (blocks extracted from frames) | `python3 -I test/fixtures/generate_lz4_fixtures.py [path/to/lz4]` |
| `HeatshrinkCodec` | reference heatshrink (PyPI `heatshrink2`, windows 4..13) | `pip install heatshrink2`, then `python3 -I test/fixtures/generate_heatshrink_fixtures.py` |

RLE, Delta and LZSS have no standard external format or tool, so they are covered by round-trip and known-vector tests only. See [Codecs](Codecs.md) for standards and tested platforms.

## Arduino

The example sketch compiles with `arduino-cli` for AVR and other cores:

```bash
arduino-cli compile --library . --fqbn arduino:avr:uno examples/RoundTrip
```
