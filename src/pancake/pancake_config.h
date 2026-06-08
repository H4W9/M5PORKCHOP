#pragma once
// =============================================================
//  PORKCHOP PANCAKE — Hardware Configuration
//  Target: ESP32-C5-DevKitC-1
//  Display: ST7796 3.5" 320x480 portrait native
//  Touch:   FT6336 capacitive I2C
//
//  Pin assignments match User_Setup_marauder_pancake.h
// =============================================================

// ---- TFT_eSPI SPI pins (from User_Setup_marauder_pancake.h) -
#define PANCAKE_TFT_MISO   4
#define PANCAKE_TFT_MOSI  24
#define PANCAKE_TFT_CLK   23
#define PANCAKE_TFT_CS     5
#define PANCAKE_TFT_DC     3
#define PANCAKE_TFT_RST    2
#define PANCAKE_TFT_BL    26   // active HIGH

// ---- FT6336 capacitive touch (I2C) -------------------------
#define PANCAKE_TOUCH_SDA  15
#define PANCAKE_TOUCH_SCL  16
#define PANCAKE_TOUCH_INT  17   // active LOW, optional
#define PANCAKE_TOUCH_RST  -1
#define FT6336_I2C_ADDR    0x38

// ---- SD card -----------------------------------------------
#define PANCAKE_SD_CS       8   // adjust if your wiring differs

// ---- NeoPixel ----------------------------------------------
#define PANCAKE_LED_PIN    38
#define PANCAKE_LED_COUNT   1

// ---- Display geometry --------------------------------------
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
