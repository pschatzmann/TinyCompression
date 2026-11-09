# API

All classes live in the `TinyCompression` namespace. On Arduino, `TinyCompression.h` imports the namespace automatically (`using namespace TinyCompression;`), so sketches can write `HeatshrinkCodec codec;` without the prefix. The examples in this document use the full name for clarity; desktop code must use the prefix or its own `using` declaration.

## Codecs

Every codec derives from `TinyCompression::Codec`:

| Class | Constructor | Notes |
|---|---|---|
| `RleCodec` | `()` | Run-length, PackBits style |
| `DeltaCodec` | `()` | Differences between bytes, 4-bit codes |
| `LzssCodec` | `()` | Classic LZSS, 4 KB window |
| `HeatshrinkCodec` | `(windowBits = 8, lookaheadBits = 4)` | Configurable LZSS |
| `Lz4Codec` | `()` | LZ4 block format |

Codec objects are not copied by the streams; keep them alive for as long as the stream uses them. A codec instance can be shared by several streams that are used one after another, but not concurrently.

## RamStream

```cpp
TinyCompression::RamStream ram;  // a Stream backed by std::vector<uint8_t>
```

A `Stream` that stores written bytes in `ram.data` and reads them back from `ram.rpos`. It is useful for tests and for keeping compressed data in memory. It needs `std::vector`, so it is not available on AVR boards (Uno, Nano, ...); on those, provide your own fixed-buffer stream as `examples/RoundTrip` does.

| Member | Description |
|---|---|
| `data` | `std::vector<uint8_t>` holding the stored bytes. |
| `rpos` | Read position. Set to `0` to read from the start again. |
| `clear()` | Removes all data and resets `rpos`. |

## CompressionWriter

```cpp
CompressionWriter(Print& out, Codec& codec, size_t blockSize = 256);
```

A `Print`, so `write()`, `print()` and `println()` work. Bytes are collected into a block; each time a block of `blockSize` bytes is full, it is compressed and written to `out` as one frame. Nothing larger than one block is held in memory.

| Method | Description |
|---|---|
| `flush()` | Emits the pending partial block and flushes `out`. Call it when you are done writing. |
| `ok()` | `false` if memory allocation failed or a write to `out` failed. |
| `bytesIn()` | Uncompressed bytes accepted so far. |
| `bytesOut()` | Bytes sent to `out` so far (frame headers included). |
| `blockSize()` | Effective block size (clamped to 1..32768). |

Use a smaller block size for lower RAM use and faster response; use a larger one for better compression. Compression ratios are usually better with larger blocks, and LZ-based codecs search only within the current block.

## CompressionReader

```cpp
CompressionReader(Stream& in, Codec& codec, size_t blockSize = 256);
```

A `Stream`, so `read()`, `peek()`, `available()` and `readBytes()` work. Frames are read from `in` and decoded when the reader needs the next byte. The reader's `blockSize` must be at least the writer's block size.

| Method | Description |
|---|---|
| `available()` | Decoded bytes ready to read. Decodes the next frame if the buffer is empty and a full frame header is available on `in`. |
| `read()` / `peek()` | Next byte, or `-1` if no data is available (end of data, or an error). |
| `readBytes(buf, len)` | Reads up to `len` bytes; stops at end of data or on error. |
| `error()` | `true` after a malformed frame, a codec mismatch, a truncated frame, or a block size that is too small. |
| `ok()` | `false` if memory allocation failed or an error occurred. |

The reader uses `in.readBytes()` to read a complete frame once its header is available, so `in` should have a sensible timeout set (`in.setTimeout(...)`) when it is a serial or network stream.

## Example: compress to a file, decompress later

```cpp
#include <SD.h>
#include <TinyCompression.h>

TinyCompression::Lz4Codec codec;

void saveLog(const char* text) {
  File f = SD.open("log.tcz", FILE_WRITE);
  TinyCompression::CompressionWriter w(f, codec, 512);
  w.print(text);
  w.flush();
  f.close();
}

void loadLog() {
  File f = SD.open("log.tcz", FILE_READ);
  TinyCompression::CompressionReader r(f, codec, 512);
  while (r.available()) Serial.write((char)r.read());
  f.close();
}
```

See `examples/RoundTrip` for a complete sketch.

## Implementing a custom codec

Derive from `TinyCompression::Codec` and implement:

- `id()`: a unique value from 1 to 255 (0 is reserved for stored blocks). The reader rejects frames whose id does not match the codec.
- `name()`: a display name.
- `encode(src, n, dst, cap)`: return the encoded size, or 0 if the result does not fit into `cap`.
- `decode(src, n, dst, dstLen)`: produce exactly `dstLen` bytes; return `false` on malformed input. It must never read or write outside the given buffers.
