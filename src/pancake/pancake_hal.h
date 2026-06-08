#pragma once
// =============================================================
//  pancake_hal.h  —  Minimal force-include shim
//  PORKCHOP PANCAKE (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  Force-included into EVERY TU (.cpp AND .c) via -include.
//  Must be valid C — NimBLE and other libs have .c files.
//  Must include NOTHING — not Arduino.h, not TFT_eSPI.h, nothing.
//
//  All actual types and stubs live in:
//    src/pancake/stubs/M5Unified.h    (found via -I src/pancake/stubs)
//    src/pancake/pancake_hal_impl.h   (included by display.h)
// =============================================================

/* No includes, no types — just the bare minimum to make the
   build system aware this is the Pancake target. */
