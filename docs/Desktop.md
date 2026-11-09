# Desktop

TinyCompression is header-only, so there is nothing to compile for the library itself. The desktop build compiles the test suite and the example sketch with a standard C++11 compiler. CMake 3.13 or newer is needed.

## Building with CMake

```bash
cmake -S . -B build
cmake --build build
```

This builds two programs:

- `tinycompression_tests`: the automated test suite.
- `tinycompression_roundtrip`: the `examples/RoundTrip` sketch, built for the desktop (see below).

CMake options:

| Option | Default | Effect |
|---|---|---|
| `TC_BUILD_TESTS` | `ON` | Build the test suite |
| `TC_BUILD_EXAMPLES` | `ON` | Build the RoundTrip example |
| `TC_SANITIZE` | `OFF` | Build the tests with AddressSanitizer and UndefinedBehaviorSanitizer |

To run everything:

```bash
cd build && ctest --output-on-failure
```

Both CTest tests must pass. The example test checks that the output contains `OK`.

To build with sanitizers, use a separate build directory (ignored by `.gitignore`):

```bash
cmake -S . -B build-asan -DTC_SANITIZE=ON
cmake --build build-asan
./build-asan/tinycompression_tests
```

The sanitizer build is what catches out-of-bounds accesses in the decoders during the garbage-input test.

## Using the library in a desktop program

The CMake interface target `TinyCompression` adds the `src` include directory and sets `TC_DESKTOP`. Link against it and include the umbrella header:

```cmake
add_subdirectory(TinyCompression)
target_link_libraries(my_app PRIVATE TinyCompression)
```

```cpp
#include <TinyCompression.h>
TinyCompression::Lz4Codec codec;
```

Without CMake, add `src` to the include path. You do not need to define anything else, because `Platform.h` selects the desktop headers whenever `ARDUINO` is not defined. Define `TC_DESKTOP` explicitly to force the desktop headers even when `ARDUINO` is defined.

## Desktop stand-ins for Arduino

`Platform.h` uses the Arduino core when `ARDUINO` is defined (and `TC_DESKTOP` is not). Otherwise it uses the headers in `src/tinycompression/desktop/`:

- `Print.h` and `Stream.h`: the `Print` and `Stream` subset that the library uses, plus the common `print` and `println` overloads.
- `Arduino.h`: `Serial`, which writes to stdout, and `millis()`. Include it to run Arduino sketches on the desktop.

## Running the RoundTrip sketch

`examples/desktop/main.cpp` includes `examples/RoundTrip/RoundTrip.ino` unchanged and calls `setup()` and `loop()` from `main()`. The sketch's output is written to stdout:

```
input bytes: 104  compressed bytes: 45
decoded:
TinyCompression compresses TinyCompression streams. TinyCompression compresses TinyCompression streams.
OK
```

The example uses `using namespace TinyCompression;` before including the sketch, because on Arduino the umbrella header adds that import.

## What is tested

- **Known vectors:** RLE, Delta and LZ4 outputs for fixed inputs.
- **Round trips:** every codec on zeros, ramps, text, random data, runs and all byte values, at sizes around the block and token boundaries.
- **Capacity:** encoders return 0 instead of overflowing a too-small buffer.
- **Robustness:** random, truncated and bit-flipped input never crashes a decoder.
- **Streams:** writer and reader round trips across block sizes, incremental reads, `readBytes()`, `flush()` emitting partial blocks, stored fallback overhead, compression of runs, codec mismatch, truncated input, and a reader block size that is too small.
- **External fixtures:** data produced by external tools, decoded with the library (`test/test_external.cpp`).

### External fixtures

| Codec | Source of fixtures | Regenerate with |
|---|---|---|
| `Lz4Codec` | reference `lz4` CLI v1.9+ (blocks extracted from frames) | `python3 -I test/fixtures/generate_lz4_fixtures.py [path/to/lz4]` |
| `HeatshrinkCodec` | reference heatshrink (PyPI `heatshrink2`, windows 4..13) | `pip install heatshrink2`, then `python3 -I test/fixtures/generate_heatshrink_fixtures.py` |

RLE, Delta and LZSS have no standard external format or tool, so they are covered by round-trip and known-vector tests only. See [Codecs](Codecs.md) for standards.
