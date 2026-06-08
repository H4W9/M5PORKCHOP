#pragma once
// M5Unified.h stub for PORKCHOP PANCAKE
// Redirects to the single authoritative type definitions in pancake_hal_impl.h.
// TFT_eSPI.h is findable via -I lib/TFT_eSPI/TFT_eSPI-ESP32-C5 in build_flags.
#ifdef PORKCHOP_PANCAKE
  #include "../pancake_hal_impl.h"
#endif
