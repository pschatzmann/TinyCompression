#pragma once
// TinyCompression: header-only compression codecs with Arduino Print/Stream wrappers.
#include "tinycompression/Platform.h"
#include "tinycompression/Codec.h"
#include "tinycompression/CompressionStream.h"
#include "tinycompression/RamStream.h"
#include "tinycompression/codecs/RleCodec.h"
#include "tinycompression/codecs/DeltaCodec.h"
#include "tinycompression/codecs/LzssCodec.h"
#include "tinycompression/codecs/HeatshrinkCodec.h"
#include "tinycompression/codecs/Lz4Codec.h"

// Arduino sketches can use the classes without the TinyCompression:: prefix.
#if defined(ARDUINO)
using namespace TinyCompression;
#endif
