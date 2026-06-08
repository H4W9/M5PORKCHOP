#pragma once
// =============================================================
//  pancake_hal.h  —  Minimal force-include shim
//  PORKCHOP PANCAKE (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  Force-included into EVERY translation unit (both .cpp and .c)
//  via -include in platformio.ini.
//
//  Rules for this file:
//   - Must be valid C (not just C++) — NimBLE has .c files
//   - No C++ headers (<string>, <vector>, etc.)
//   - No Arduino.h, TFT_eSPI.h, or any library header
//   - Only preprocessor defines and C-compatible declarations
//
//  All it needs to do is block the M5Stack headers everywhere.
//  Full HAL types live in pancake_hal_impl.h, included only
//  from porkchop's own .cpp files via display.h.
// =============================================================

#ifdef PORKCHOP_PANCAKE

/* Block real M5Stack headers before they can be pulled in */
#define _M5CARDPUTER_H_
#define _M5UNIFIED_H_
#define _M5GFX_H_

/*
 * lgfxFont_t forward declaration.
 * display.h references this type in a function signature.
 * Must be C-compatible (no struct body in C would cause issues,
 * but an empty struct is fine in C++ and this only matters in
 * .cpp files anyway — in .c files this define is never used).
 */
#ifdef __cplusplus
struct lgfxFont_t {};
#endif

#endif /* PORKCHOP_PANCAKE */
