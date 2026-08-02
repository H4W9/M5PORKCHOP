#pragma once
// =============================================================
//  PORKCHOP PANCAKE — Hardware Configuration
//  Target: ESP32-C5-DevKitC-1
//  Display: ST7796 3.5" 320x480 portrait native
//  Touch:   FT6336 capacitive I2C
//
//  Pin assignments match User_Setup_marauder_pancake.h
// =============================================================

// ---- TFT_eSPI SPI pins -------------------------------------
// (Consumed by TFT_eSPI via the board User_Setup; mirrored here for any
//  direct GPIO use such as backlight control.)
#ifdef PORKCHOP_MARAUDER_V8
  // Marauder V8 — from User_Setup_marauder_V8.h
  #define PANCAKE_TFT_MISO   2
  #define PANCAKE_TFT_MOSI   7
  #define PANCAKE_TFT_CLK    6
  #define PANCAKE_TFT_CS    23
  #define PANCAKE_TFT_DC    24
  #define PANCAKE_TFT_RST   -1   // tied to EN in hardware
  #define PANCAKE_TFT_BL     8   // active HIGH
#else
  // Pancake — from User_Setup_marauder_pancake.h
  #define PANCAKE_TFT_MISO   4
  #define PANCAKE_TFT_MOSI  24
  #define PANCAKE_TFT_CLK   23
  #define PANCAKE_TFT_CS     5
  #define PANCAKE_TFT_DC     3
  #define PANCAKE_TFT_RST    2
  #define PANCAKE_TFT_BL    26   // active HIGH
#endif

// ---- Touch + I2C -------------------------------------------
#ifdef PORKCHOP_MARAUDER_V8
  // XPT2046 resistive touch on the shared FSPI bus (chip-select below), driven
  // by TFT_eSPI's getTouch(). Needs calibration (stored in prefs). The I2C bus
  // here carries only the MAX17048 battery gauge.
  #define PANCAKE_XPT2046_CS  3
  #define PANCAKE_I2C_SDA     5
  #define PANCAKE_I2C_SCL     4
#else
  // FT6336 capacitive touch (I2C)
  #define PANCAKE_TOUCH_SDA   9
  #define PANCAKE_TOUCH_SCL  10
  #define PANCAKE_TOUCH_INT  -1
  #define PANCAKE_TOUCH_RST   8
  #define FT6336_I2C_ADDR    0x38
  #define PANCAKE_I2C_SDA     PANCAKE_TOUCH_SDA
  #define PANCAKE_I2C_SCL     PANCAKE_TOUCH_SCL
#endif

// ---- SD card -----------------------------------------------
#ifdef PORKCHOP_MARAUDER_V8
  #define PANCAKE_SD_CS      10
#else
  #define PANCAKE_SD_CS       7
#endif

// ---- Status LED --------------------------------------------
#ifdef PORKCHOP_MARAUDER_V8
  // Single blue activity LED, active-high (not addressable RGB).
  #define PANCAKE_LED_PIN         28
  #define PANCAKE_LED_ACTIVE_HIGH  1
#elif defined(RGB_BUILTIN)
  #define PANCAKE_LED_PIN  RGB_BUILTIN
#else
  #define PANCAKE_LED_PIN  LED_BUILTIN
#endif
#define PANCAKE_LED_COUNT   1

// ---- Piezo buzzer ------------------------------------------
// Pancake: passive piezo driven by Arduino tone() (LEDC PWM).
//   Wire GPIO -> buzzer(+) -> buzzer(-) -> GND (optional 100ohm in series).
//   GPIO25 is free, non-strapping, and broken out; change here if needed
//   (GPIO15 or GPIO22 are equally safe alternates).
//
//   >>> No buzzer fitted? Comment out PANCAKE_BUZZER_ENABLED below and the
//       speaker falls back to a silent stub (all SFX become no-ops). <<<
//
// V8: no buzzer hardware -> always silent.
#ifdef PORKCHOP_MARAUDER_V8
  #define PANCAKE_BUZZER_PIN  -1
#else
  #define PANCAKE_BUZZER_ENABLED       // <-- comment out if no buzzer wired
  #ifdef PANCAKE_BUZZER_ENABLED
    #define PANCAKE_BUZZER_PIN  25
  #else
    #define PANCAKE_BUZZER_PIN  -1
  #endif
#endif

// ---- Display geometry --------------------------------------
#ifdef PORKCHOP_MARAUDER_V8
// ILI9341 240x320 portrait (rotation=0).
#define PANCAKE_ROTATION    0
#define PANCAKE_SCREEN_W  240
#define PANCAKE_SCREEN_H  320

// Same split model as Pancake, scaled for 240x320. The content pane is only
// 135px (matches the avatar scene: MAIN_H=107, so the grass sits just above the
// bottom bar with no empty space below it); the full Pancake keyboard fills the
// bottom 185px (over half the screen), always visible.
#define PANCAKE_PORK_H      135   // content pane (topBar 14 + main 107 + bottomBar 14)
#define PANCAKE_KB_Y        135   // keyboard pane top edge
#define PANCAKE_KB_H        185   // keyboard pane height (135..320)
#define PANCAKE_KB_ROW_H     36   // 5 rows fit in 185px
#define PANCAKE_KB_MARGIN     3

#define PANCAKE_TOUCH_W   240
#define PANCAKE_TOUCH_H   320
#else
// ST7796 is 320x480 portrait natively (TFT_WIDTH=320, TFT_HEIGHT=480)
// setRotation(0) = portrait, (1) = landscape, etc.
// We use portrait, rotation=0.
#define PANCAKE_ROTATION    0

// Logical screen dimensions (portrait)
#define PANCAKE_SCREEN_W  320
#define PANCAKE_SCREEN_H  480

// Split: top 240 px = porkchop content, bottom 240 px = keyboard
#define PANCAKE_PORK_H    240   // porkchop pane height
#define PANCAKE_KB_Y      240   // keyboard pane starts here
#define PANCAKE_KB_H      240   // keyboard pane height

// Keyboard layout
#define PANCAKE_KB_ROW_H   44
#define PANCAKE_KB_MARGIN   3

// Touch coordinate range
#define PANCAKE_TOUCH_W   320
#define PANCAKE_TOUCH_H   480
#endif
