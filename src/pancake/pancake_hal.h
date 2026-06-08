#pragma once
// =============================================================
//  pancake_hal.h  —  Lightweight force-include shim
//  PORKCHOP PANCAKE (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  This file IS force-included into every TU via -include.
//  It must NOT include TFT_eSPI.h here — doing so causes
//  "TFT_eSPI.h: No such file or directory" errors when the
//  compiler processes library files (SPI.cpp, FS.cpp, etc.)
//  before the library search paths are configured.
//
//  What this file does:
//   1. Defines M5Cardputer/M5Unified/M5GFX include guards so
//      those real headers are silently skipped everywhere.
//   2. When a porkchop source file then tries to include
//      <M5Cardputer.h> or <M5Unified.h>, those includes are
//      no-ops, BUT the file also gets the full HAL via the
//      "redirect" mechanism in pancake_hal_impl.h which IS
//      safe to include (only included by files that already
//      have TFT_eSPI in scope).
// =============================================================

#ifdef PORKCHOP_PANCAKE

// ---- Block real M5Stack headers ----------------------------
// Must be defined before any TU can pull them in.
#define _M5CARDPUTER_H_
#define _M5UNIFIED_H_
#define _M5GFX_H_

// lgfx font type forward declaration (used in display.h)
struct lgfxFont_t {};

// ---- Minimal Arduino / FreeRTOS stubs ----------------------
// These are always available in the Arduino framework so safe
// to include even in library TUs.
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string>

// ---- KEY MECHANISM -----------------------------------------
// Any porkchop source file that used to do:
//   #include <M5Cardputer.h>
// now gets nothing (guard fires). But those same files include
// "pancake_hal_impl.h" explicitly (via a second -include flag
// that only fires AFTER the lib search paths are ready), or
// they get it transitively through display.h / porkchop.h
// which both include it.
//
// See: pancake_hal_impl.h for the full type definitions.

#endif // PORKCHOP_PANCAKE
