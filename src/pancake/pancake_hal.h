#pragma once
// =============================================================
//  pancake_hal.h  —  Minimal force-include shim
//  PORKCHOP PANCAKE (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  Force-included into EVERY translation unit (.cpp AND .c)
//  via -include in platformio.ini.
//
//  Must be valid C — NimBLE, esp-hci and other libraries have
//  .c files that get this header injected too.
//
//  The real work (blocking M5Unified/M5Cardputer, providing shim
//  types) is done by stub headers in src/pancake/stubs/ which
//  shadow the real library headers on the include path.
//  See: src/pancake/stubs/M5Unified.h
//       src/pancake/stubs/M5Cardputer.h
//
//  This file intentionally contains almost nothing.
// =============================================================

#ifdef PORKCHOP_PANCAKE

#ifdef __cplusplus
// lgfxFont_t is referenced in display.h before pancake_hal_impl.h
// is included, so we forward-declare it here.
struct lgfxFont_t {};
#endif

#endif /* PORKCHOP_PANCAKE */
