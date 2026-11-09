#!/usr/bin/env python3
"""Generates Heatshrink test fixtures with the reference heatshrink library
(PyPI package `heatshrink2`, which wraps the C implementation).

Writes into this folder:
  heatshrink_<name>.raw            - original data
  heatshrink_<name>.w<W>l<L>.hs    - heatshrink output for window_sz2=W, lookahead_sz2=L

Setup:  pip install heatshrink2
Usage:  python3 -I generate_heatshrink_fixtures.py
The inputs are deterministic (fixed seed).
"""
import os
import random

import heatshrink2

HERE = os.path.dirname(os.path.abspath(__file__))


def build_cases():
    rnd = random.Random(20260102)
    words = [b"sensor", b"value", b"temp", b"42", b"tiny", b"stream", b"data "]
    return {
        "text": b" ".join(rnd.choice(words) for _ in range(900)),
        "runs": b"".join(bytes([rnd.randrange(256)]) * rnd.randrange(1, 60)
                         for _ in range(300)),
        "random": bytes(rnd.randrange(256) for _ in range(2000)),
        "ramp": bytes(i % 256 for i in range(3000)),
    }


# (window_sz2, lookahead_sz2) pairs covering small, default and large settings.
# The reference package accepts lookahead 3..7 and needs lookahead < window.
PARAMS = [(4, 3), (8, 4), (10, 6), (12, 5), (13, 7)]


def main():
    for name, data in build_cases().items():
        with open(os.path.join(HERE, f"heatshrink_{name}.raw"), "wb") as f:
            f.write(data)
        for w, l in PARAMS:
            enc = heatshrink2.compress(data, window_sz2=w, lookahead_sz2=l)
            # Sanity check with the reference decoder before writing the fixture.
            assert heatshrink2.decompress(enc, window_sz2=w, lookahead_sz2=l) == data
            with open(os.path.join(HERE, f"heatshrink_{name}.w{w}l{l}.hs"), "wb") as f:
                f.write(enc)
            print(f"{name:8s} w={w:2d} l={l}: {len(data)} -> {len(enc)} bytes")


if __name__ == "__main__":
    main()
