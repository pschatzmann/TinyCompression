# Codecs

| Codec | id | Best for | Compression | Working memory | Standardization |
|---|---|---|---|---|---|
| `RleCodec` | 1 | Long runs of identical bytes (sparse data, padding) | Good on runs, expands otherwise | none | Custom, PackBits-style; not byte-compatible with PackBits |
| `DeltaCodec` | 2 | Slowly changing values (ramps, sensor readings) | About 50% on small differences | none | Custom; no external reference |
| `LzssCodec` | 3 | General data with repeated substrings | Good | none (brute-force search in the block) | Custom LZSS; no external reference |
| `HeatshrinkCodec` | 4 | General data on small MCUs; tunable | Good, depends on window/lookahead | none (brute-force search in the block) | Compatible with the reference heatshrink library (verified) |
| `Lz4Codec` | 5 | General data, output compatible with the reference `lz4` tool | Good | 2 KB hash table | LZ4 block format (verified against `lz4` 1.9.4) |

Incompressible blocks are stored raw automatically, so no codec expands the stored data by more than 5 bytes per block.

## RleCodec

PackBits-style run-length coding. A header byte below 128 introduces `h+1` literal bytes; a header of 128 or above repeats the next byte `h-126` times (2 to 129). Its run headers differ from PackBits, which uses 129..255 for runs of 2..128, so the two are not interchangeable.

## DeltaCodec

Each byte is coded as the difference to the previous byte, starting from 0. Differences from -7 to +7 take 4 bits; all other differences take 12 bits (an escape code followed by the 8-bit difference). Use it for data where neighbouring values are close, such as a temperature that changes by a few units per sample.

## LzssCodec

Classic LZSS with a flag bit per item: a literal costs 9 bits, a back-reference costs 17 bits and covers 3 to 18 bytes at distances of up to 4095. Matches never cross block boundaries.

## HeatshrinkCodec

LZSS with configurable field widths:

- `windowBits` (4 to 13): the back-reference window is `2^windowBits` bytes.
- `lookaheadBits` (2 to `windowBits - 1`): the maximum match length is `2^lookaheadBits`.

The defaults (8 and 4) suit most microcontrollers. Larger windows find more matches but make the search slower and the codec's bit fields wider. Encoder and decoder must use the same values, because the codec id does not encode them.

## Lz4Codec

The standard LZ4 block format (without the frame wrapper), so blocks produced by the reference `lz4` tool can be decoded. The encoder uses a hash table of 1024 entries (2 KB of RAM). On a board with 2 KB of RAM such as the Uno, that table alone uses all of it (see [Arduino platforms](#arduino-platforms)).

## Choosing a block size

- Small blocks (16 to 256 bytes): low RAM, quick response, poorer ratios.
- Large blocks (1 KB to 4 KB): better ratios, more RAM for both the writer and the reader (two buffers each).
- LZ-family encoders search linearly within the block, so very large blocks slow writing down; keep them at a few KB at most on small boards.

## Standards and interoperability

- **Standard formats:** `Lz4Codec` uses the LZ4 block format. Its blocks from the reference `lz4` CLI v1.9.4 decode correctly (six inputs, two levels each). `HeatshrinkCodec` decodes output from the reference heatshrink library (via the `heatshrink2` package) for five window/lookahead settings.
- **Library-specific formats:** `RleCodec`, `DeltaCodec` and `LzssCodec` have no standard external format or tool here, so they are checked only by round-trip and known-vector tests and interoperate only with this library.
- **Frames:** the 5-byte frame header is TinyCompression-specific; see [Format](Format.md). The codec id in each frame lets the reader reject a stream written with a different codec. It does not record Heatshrink's window and lookahead sizes, so both sides must agree on them.
- **Regenerating the fixtures:** see [Desktop](Desktop.md#external-fixtures).

## Language and build

- **Language:** C++11. The codecs and stream classes use only C headers (`stdint.h`, `stddef.h`, `stdlib.h`, `string.h`) and no exceptions. The one exception is `RamStream`, which stores its data in a `std::vector` and is not available on AVR.
- **Memory:** the codecs do not allocate memory. The streams allocate two buffers of `blockSize` bytes each with `malloc()` in their constructors.
- **Dependencies:** none beyond the C++11 compiler and the Arduino core (or the standard headers on the desktop).
- **Compilers tested:** g++ 13 on Linux with `-Wall -Wextra -Wpedantic` and no warnings; the test suite also runs under AddressSanitizer and UBSan.
- **Not tested:** clang, MSVC, and C++14 or later.
- **Byte order:** frame lengths and LZ4 offsets are written byte by byte, so the format does not depend on host endianness. Big-endian hardware has not been tested.

## Arduino platforms

Sketch used for the checks: all five codecs in one sketch, plus the `examples/RoundTrip` sketch for the ESP32 and RP2040.

| Core | Status |
|---|---|
| `arduino:avr:uno` | Overflows RAM by about 300 bytes with all five codecs, mainly because of the LZ4 table. A sketch with only `HeatshrinkCodec` fits. `RamStream` is not available; `examples/RoundTrip` uses a fixed-buffer stream on AVR. |
| `arduino:avr:mega` | All five codecs fit with room to spare. |
| `esp32:esp32:esp32` (core 3.3.12) | Compiles without warnings; about 24 KB of 320 KB RAM. Not run on hardware. |
| `rp2040:rp2040:rpipico` (core 6.3.2) | Compiles without warnings; about 11 KB of 256 KB RAM. Not run on hardware. |
| Other cores (SAMD, nRF52, STM32, Teensy, ...) | Not tested. |

Known platform difference: on the AVR core, `Stream::readBytes` is not virtual, so calling `readBytes` through a `Stream&` reference uses the core's implementation rather than `CompressionReader`'s.

To compile the example for a board:

```bash
arduino-cli compile --library . --fqbn arduino:avr:uno examples/RoundTrip
```

## Open items

- LZ4's 2 KB hash table is the reason it does not fit on an Uno. A 256-entry table would take 512 bytes, at some cost in compression.
- RLE, Delta and LZSS have no external reference in this environment.
- The ESP32 and RP2040 builds have not been run on hardware.
