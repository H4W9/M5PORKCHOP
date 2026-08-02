//                            USER DEFINED SETTINGS
//   TFT_eSPI setup for Marauder V8 (ESP32-C5 + ILI9341 2.8" 240x320 + XPT2046)
//   Pins confirmed against ESP32_FlipSocial User_Setup_marauder_v8.h.

// ##################################################################################
// Section 1. Driver
// ##################################################################################

#define ILI9341_DRIVER

// 2.8-inch portrait panel native resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// V8 panel uses RGB colour order — without this RED shows as BLUE. TFT_BGR
// pre-swaps R/B, cancelling the ILI9341's default MADCTL BGR=1.
#define TFT_RGB_ORDER TFT_BGR

// ##################################################################################
// Section 2. Pin assignments (V8 hardware — shared FSPI bus: TFT + SD + touch)
// ##################################################################################

#define TFT_MISO  2
#define TFT_MOSI  7
#define TFT_SCLK  6
#define TFT_CS    23   // TFT chip select
#define TFT_DC    24   // Data/command
#define TFT_RST   -1   // Reset tied to EN via hardware — no GPIO needed
#define TFT_BL     8   // Backlight (PWM, active HIGH)
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS   3   // XPT2046 resistive touch controller

// ##################################################################################
// Section 3. Fonts
// ##################################################################################

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
// #define LOAD_FONT6
// #define LOAD_FONT7
// #define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ##################################################################################
// Section 4. SPI speed
// ##################################################################################

#define SPI_FREQUENCY       20000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000   // XPT2046 max is 2.5 MHz
