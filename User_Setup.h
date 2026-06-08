// TFT_eSPI User_Setup.h — PORKCHOP PANCAKE
// ESP32-C5-DevKitC-1 + ST7796 3.5" 480x320
// Portrait mode via setRotation(2): logical 320x480
//
// Place this file in the TFT_eSPI library folder,
// OR keep in project root — platformio.ini uses -include to pull it in.

#define USER_SETUP_LOADED 1
#define USER_SETUP_INFO "PORKCHOP_PANCAKE_ST7796"

// ---- Display driver ----------------------------------------
#define ST7796_DRIVER

// ---- Physical resolution (landscape native) ----------------
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// ---- SPI pins (HSPI on ESP32-C5-DevKitC-1) -----------------
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCLK 12
#define TFT_CS    4
#define TFT_DC    5
#define TFT_RST   6

// Backlight — controlled in software via analogWrite
#define TFT_BL          7
#define TFT_BACKLIGHT_ON HIGH

// ---- SPI frequency -----------------------------------------
#define SPI_FREQUENCY      40000000
#define SPI_READ_FREQUENCY  6000000

// ---- Fonts --------------------------------------------------
#define LOAD_GLCD  1
#define LOAD_FONT2 1
#define LOAD_FONT4 1
#define LOAD_FONT6 1
#define LOAD_FONT7 1
#define LOAD_FONT8 1
#define LOAD_GFXFF 1
#define SMOOTH_FONT 1

// ---- ESP32-C5: use SPI2 (HSPI) host -----------------------
// ESP32-C5 Arduino maps HSPI to SPI2
#define USE_HSPI_PORT 1
