# TinyCompression

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![CMake](https://img.shields.io/badge/CMake-Supported-blue.svg)](CMakeLists.txt)
[![Header Only](https://img.shields.io/badge/Header-Only-blue.svg)](src/TinyCompression.h)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## Introduction

Microcontrollers have little RAM and slow links (UART, BLE, LoRa, flash, SD). Compressing data before it leaves the chip saves bandwidth and storage, but most compression libraries only offer whole-buffer APIs that do not fit the Arduino stream model.

TinyCompression wraps compression around Arduino's `Print` and `Stream` interfaces. You write data as usual, and it is compressed on the way out; you read data as usual, and it is decompressed on the way in. The codec (RLE, Delta, LZSS, Heatshrink or LZ4) is chosen with one constructor argument, so switching algorithms does not change the surrounding code. The library is header-only and also builds on the desktop, so codecs can be tested with `ctest`.

TinyCompression is for generic data (logs, sensor records, configuration, binary blobs). For audio, use the codecs in [AudioTools](https://github.com/pschatzmann/arduino-audio-tools) instead.

## Documentation

Detailed documentation is in the [docs](docs/) folder:

- [API](docs/API.md): classes, constructors and usage examples
- [Codecs](docs/Codecs.md): the available algorithms, their parameters and trade-offs
- [Format](docs/Format.md): the on-wire frame format
- [Testing](docs/Testing.md): desktop build and automated tests

## Installation in Arduino

You can download the library as ZIP and use *Sketch → Include Library → Add .ZIP Library...*. Or you can clone the project into your Arduino libraries folder, e.g.

```
cd ~/Documents/Arduino/libraries
git clone <repository-url> TinyCompression
```

I recommend git, because you can update to the latest version with `git pull` in the project folder.

With `arduino-cli`, either place the library in your sketchbook's `libraries` folder or pass its location with `--library <path>` when compiling. Tested boards are listed in [Codecs](docs/Codecs.md#arduino-platforms).

Then include it in a sketch:

```cpp
#include <TinyCompression.h>
```
