#!/usr/bin/env python3
"""Generates LZ4 test fixtures with the reference `lz4` command-line tool.

For each case it writes two files into this folder:
  <name>.raw  - the original (uncompressed) data
  <name>.lz4  - one LZ4 block as produced by the reference tool (frame wrapper removed)

Usage: python3 -I generate_lz4_fixtures.py [path/to/lz4]
Requires the lz4 CLI (v1.9+). The inputs are deterministic (fixed seed), so
regenerating produces identical data.
"""
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LZ4 = sys.argv[1] if len(sys.argv) > 1 else "lz4"


def build_cases():
    rnd = random.Random(20260101)
    words = [b"sensor", b"value", b"temp", b"42", b"tiny", b"stream", b"data "]
    cases = {}
    cases["text"] = b" ".join(rnd.choice(words) for _ in range(900))
    cases["runs"] = b"".join(
        bytes([rnd.randrange(256)]) * rnd.randrange(1, 60) for _ in range(300))
    cases["ramp"] = bytes(i % 256 for i in range(3000))
    # Long run: match length needs extension bytes (>= 15 + 255 + ...).
    cases["zeros_long"] = bytes(70000 // 2)
    # Long literal run (random) followed by a long match: literal length extension.
    cases["literal_then_match"] = bytes(rnd.randrange(256) for _ in range(600)) + b"Z" * 2000
    # End-of-block rules (last 5 bytes literal, last match starts 12 bytes before the end).
    cases["abcd_repeat"] = b"abcd" * 16
    return cases


def lz4_block(path_in, level):
    out = subprocess.run([LZ4, "-q", "-c", f"-{level}", "--no-frame-crc", path_in],
                         check=True, stdout=subprocess.PIPE).stdout
    if out[:4] != b"\x04\x22\x4d\x18":
        raise RuntimeError("not an LZ4 frame")
    flg = out[4]
    pos = 4 + 2 + (8 if flg & 0x08 else 0) + 1  # magic, FLG, BD, [content size], header checksum
    size = struct.unpack("<I", out[pos:pos + 4])[0]
    pos += 4
    if size & 0x80000000:
        raise RuntimeError("block stored uncompressed (incompressible input)")
    return out[pos:pos + size]


def main():
    tmp = os.path.join(HERE, ".tmp_lz4_input")
    for name, data in build_cases().items():
        with open(tmp, "wb") as f:
            f.write(data)
        with open(os.path.join(HERE, name + ".raw"), "wb") as f:
            f.write(data)
        # Level 1 (fast) and level 9 (high compression) produce different token layouts.
        for level in (1, 9):
            block = lz4_block(tmp, level)
            with open(os.path.join(HERE, f"{name}.l{level}.lz4"), "wb") as f:
                f.write(block)
            print(f"{name:20s} level {level}: {len(data)} -> {len(block)} bytes")
    os.remove(tmp)


if __name__ == "__main__":
    main()
