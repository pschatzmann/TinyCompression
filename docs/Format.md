# Format

A TinyCompression stream is a sequence of frames. There is no stream header or trailer, so a stream can be produced and consumed incrementally.

## Frame

| Offset | Size | Field |
|---|---|---|
| 0 | 1 byte | Codec id. `0` = stored (payload is the raw block). Otherwise the id of the codec used. |
| 1 | 2 bytes | Raw (uncompressed) length of the block, little endian. 1 to `blockSize`. |
| 3 | 2 bytes | Payload length, little endian. Equal to the raw length for stored frames; always smaller than the raw length otherwise. |
| 5 | n bytes | Payload. |

The writer emits a frame for each block of `blockSize` bytes, and for the partial block at `flush()`. A block is stored raw whenever the codec cannot make it smaller.

## Codec payloads

Each codec's payload is self-contained: it decodes to exactly the raw length in the frame header, so no end-of-data marker is needed. Bit fields are packed most significant bit first.

- **RLE**: header byte `h`. `h < 128`: copy the next `h+1` bytes. `h >= 128`: repeat the next byte `h-126` times.
- **Delta**: for each byte, the difference `d` to the previous byte (initially 0). `d` in -7..+7: 4 bits `d+7`. Otherwise: 4 bits `1111` followed by 8 bits of `d`.
- **LZSS**: for each item, 1 flag bit. `1`: 8-bit literal. `0`: 12-bit offset (1..4095) and 4-bit `length-3`.
- **Heatshrink**: for each item, 1 flag bit. `1`: 8-bit literal. `0`: `windowBits` bits of `distance-1`, then `lookaheadBits` bits of `length-1`.
- **LZ4**: the LZ4 block format as specified by the LZ4 project. Decodable by the reference LZ4 decoder.

## Errors

The reader stops with an error (and `read()` returns `-1` from then on) when:

- a frame header declares a raw length larger than the reader's block size, or inconsistent lengths;
- a frame uses a codec id other than the reader's codec and is not stored;
- the input ends in the middle of a frame;
- the payload does not decode to exactly the raw length.
