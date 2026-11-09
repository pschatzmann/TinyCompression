#pragma once
// Platform glue: uses the Arduino core's Print/Stream when building for a board,
// and the desktop stand-ins in desktop/ otherwise. Define TC_DESKTOP (CMake does this
// for the desktop targets) to force the desktop headers.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO) && !defined(TC_DESKTOP)
#include <Arduino.h>
#else
#include "desktop/Print.h"
#include "desktop/Stream.h"
#endif
