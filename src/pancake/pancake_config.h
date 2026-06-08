#pragma once
// =============================================================
//  PORKCHOP PANCAKE — Hardware Configuration
//  Target: ESP32-C5-DevKitC-1
//  Display: ST7796 3.5" 480x320 (portrait → 320x480 logical)
//  Touch:   FT6336 capacitive I2C
// =============================================================

// ---- TFT_eSPI SPI pins (HSPI on ESP32-C5-DevKitC-1) --------
#define PANCAKE_TFT_MOSI    11
#define PANCAKE_TFT_MISO    13
#define PANCAKE_TFT_CLK     12
#define PANCAKE_TFT_CS       4
#define PANCAKE_TFT_DC       5
#define PANCAKE_TFT_RST      6
#define PANCAKE_TFT_BL       7   // active HIGH

// ---- FT6336 capacitive touch (I2C) -------------------------
#define PANCAKE_TOUCH_SDA   15
#define PANCAKE_TOUCH_SCL   16
#define PANCAKE_TOUCH_INT   17   // active LOW, optional
#define PANCAKE_TOUCH_RST   -1
#define FT6336_I2C_ADDR     0x38

// ---- SD card (shared HSPI with display) --------------------
#define PANCAKE_SD_CS        8

// ---- NeoPixel ----------------------------------------------
#define PANCAKE_LED_PIN     38
#define PANCAKE_LED_COUNT    1

// ---- Display geometry --------------------------------------
// Physical pixels: 480x320 landscape (raw panel)
// Rotation 2 gives logical portrait: 320 wide x 480 tall
#define PANCAKE_PHYS_W      480
#define PANCAKE_PHYS_H      320
#define PANCAKE_ROTATION      2   // portrait

// Logical screen in portrait mode
#define PANCAKE_SCREEN_W    320
#define PANCAKE_SCREEN_H    480

// Split: top 240 px = porkchop content, bottom 240 px = keyboard
#define PANCAKE_PORK_H      240   // porkchop pane height
#define PANCAKE_KB_Y        240   // keyboard pane starts here
#define PANCAKE_KB_H        240   // keyboard pane height

// Keyboard layout geometry
#define PANCAKE_KB_ROW_H     44   // height of each key row (5 rows x 44 = 220, fits in 240)
#define PANCAKE_KB_MARGIN     3   // gap between keys

// Touch coordinate range (matches logical portrait dimensions)
#define PANCAKE_TOUCH_W     320
#define PANCAKE_TOUCH_H     480
