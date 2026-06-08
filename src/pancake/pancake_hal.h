#pragma once
// =============================================================
//  pancake_hal.h  —  Hardware Abstraction Layer shim
//  PORKCHOP PANCAKE  (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  Force-included by platformio.ini:
//    build_flags = -include src/pancake/pancake_hal.h
//
//  Effect: every .cpp TU gets this header first, which:
//   1. Defines M5Cardputer.h / M5Unified.h / M5GFX.h include guards
//      so the real M5Stack headers are silently skipped.
//   2. Provides compatible replacement types for every API porkchop uses.
//   3. Declares (extern) the global hardware objects defined once in
//      pancake_hal.cpp.
// =============================================================

#ifdef PORKCHOP_PANCAKE

// ---- Suppress real M5Stack headers -------------------------
// These must be defined BEFORE any #include <M5Cardputer.h> etc.
#define _M5CARDPUTER_H_
#define _M5UNIFIED_H_
#define _M5GFX_H_

// lgfx font opaque type (used in display.cpp setFont calls)
struct lgfxFont_t {};

// ---- Standard / Arduino headers ----------------------------
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string>
#include <vector>
#include <functional>

// ---- Hardware libraries ------------------------------------
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SD.h>
#include <WiFi.h>

// ---- Pancake config & drivers ------------------------------
#include "pancake_config.h"
#include "FT6336Touch.h"
#include "PancakeKeyboard.h"

// ---- Global hardware objects (defined in pancake_hal.cpp) --
extern FT6336Touch     pancakeTouch;
extern PancakeKeyboard pancakeKeyboard;
extern TFT_eSPI        pancakeTFT;

// ---- M5Canvas = TFT_eSprite subclass -----------------------
// porkchop constructs:  M5Canvas Display::topBar(&M5.Display);
// TFT_eSprite(TFT_eSPI*) — we accept any pointer type and forward
// to our global TFT.
class M5Canvas : public TFT_eSprite {
public:
    template<typename T>
    explicit M5Canvas(T*) : TFT_eSprite(&pancakeTFT) {}
    M5Canvas() : TFT_eSprite(&pancakeTFT) {}

    // Font setter matching porkchop's setFont(&fonts::Font0) calls
    void setFont(const void*)        { setTextFont(1); }
    void setFont(const lgfxFont_t*)  { setTextFont(1); }

    // drawString overloads (TFT_eSprite already has these, but keep explicit)
    using TFT_eSprite::drawString;
};

// ---- fonts namespace shim ----------------------------------
// porkchop: mainCanvas.setFont(&fonts::Font0)
namespace fonts {
    static const lgfxFont_t* const Font0 = nullptr;
}

// ---- Keyboard key constants --------------------------------
#ifndef KEY_ENTER
#define KEY_ENTER      0x0D
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE  0x08
#endif

// ---- Keyboard shim -----------------------------------------
struct PancakeKeysState {
    std::string word;
    bool        del   = false;
    bool        enter = false;
};

namespace PancakeKB {
    // All state lives in pancake_hal.cpp
    extern bool    _changed;
    extern bool    _pressed;
    extern char    _ch;
    extern uint8_t _sp;
    extern bool    _polled;

    void   poll();
    void   resetPollFlag();
    bool   isChange();
    bool   isPressed();
    bool   isKeyPressed(int key);
    PancakeKeysState keysState();
}

// ---- Board ID stub -----------------------------------------
namespace m5 {
    enum class board_t : uint8_t {
        board_Unknown = 0,
        board_M5CardputerADV = 99   // never matches Pancake
    };
}

// ---- M5GFX stub (display surface) --------------------------
struct M5GFX {
    void setBrightness(uint8_t b) { analogWrite(PANCAKE_TFT_BL, b); }
    void setRotation(int)         {}
    void setColorDepth(int)       {}
    void fillScreen(uint32_t c)   {
        pancakeTFT.fillRect(0, 0, PANCAKE_SCREEN_W, PANCAKE_PORK_H, c);
    }
    void setTextColor(uint32_t fg, uint32_t bg = 0) {
        pancakeTFT.setTextColor(fg, bg);
    }
    void setTextSize(uint8_t s)   { pancakeTFT.setTextSize(s); }
    void setTextDatum(uint8_t d)  { pancakeTFT.setTextDatum(d); }
    void setCursor(int x, int y)  { pancakeTFT.setCursor(x, y); }
    void setCursor(int x, int y, uint8_t) { pancakeTFT.setCursor(x, y); }
    void print(const char* s)     { pancakeTFT.print(s); }
    void setFont(const void*)     {}
    void setFont(const lgfxFont_t*) {}
    void drawString(const char* s, int x, int y) {
        pancakeTFT.drawString(s, x, y);
    }
    void fillRect(int x, int y, int w, int h, uint32_t c) {
        if (y + h > PANCAKE_PORK_H) h = PANCAKE_PORK_H - y;
        if (h > 0) pancakeTFT.fillRect(x, y, w, h, c);
    }
    void drawFastHLine(int x, int y, int w, uint32_t c) {
        pancakeTFT.drawFastHLine(x, y, w, c);
    }
    void readRectRGB(int x, int y, int w, int h, uint8_t* buf) {
        pancakeTFT.readRectRGB(x, y, w, h, buf);
    }
    int  width()       { return PANCAKE_SCREEN_W; }
    int  height()      { return PANCAKE_PORK_H; }
    void startWrite()  { pancakeTFT.startWrite(); }
    void endWrite()    { pancakeTFT.endWrite(); }
};

// ---- Fake Keyboard_Class -----------------------------------
struct Keyboard_Class {
    using KeysState = PancakeKeysState;
    void      update()            { PancakeKB::poll(); }
    bool      isChange()          { return PancakeKB::isChange(); }
    bool      isPressed()         { return PancakeKB::isPressed(); }
    bool      isKeyPressed(int k) { return PancakeKB::isKeyPressed(k); }
    KeysState keysState()         { return PancakeKB::keysState(); }
};

// ---- Power stub --------------------------------------------
struct M5Power_Class {
    int getBatteryLevel() { return 75; }
};

// ---- Speaker stub ------------------------------------------
struct Speaker_Class {
    void tone(uint16_t, uint32_t = 1000) {}
    void stop()           {}
    void setVolume(uint8_t) {}
    bool isEnabled()      { return false; }
};

// ---- M5Cardputer_Class -------------------------------------
struct M5Cardputer_Class {
    Keyboard_Class Keyboard;
    M5GFX          Display;

    template<typename... Args>
    void begin(Args&&...) {
        // TFT init
        pancakeTFT.init();
        pancakeTFT.setRotation(PANCAKE_ROTATION);
        pancakeTFT.fillScreen(TFT_BLACK);
        pinMode(PANCAKE_TFT_BL, OUTPUT);
        digitalWrite(PANCAKE_TFT_BL, HIGH);

        // Touch init
        if (!pancakeTouch.begin()) {
            Serial.println("[PANCAKE] FT6336 not found");
        }

        // Keyboard in bottom pane — pass touch pointer so keyboard doesn't
        // need extern access to pancakeTouch
        pancakeKeyboard.begin(&pancakeTFT, &pancakeTouch);
        pancakeKeyboard.redraw();

        // Separator line between pork pane and keyboard
        pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y - 1, PANCAKE_SCREEN_W, 0x528A);
        pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y,     PANCAKE_SCREEN_W, 0x528A);

        // SD on shared HSPI — init after TFT
        SPI.begin(PANCAKE_TFT_CLK, PANCAKE_TFT_MISO, PANCAKE_TFT_MOSI,
                  PANCAKE_SD_CS);
        if (!SD.begin(PANCAKE_SD_CS, SPI, 25000000)) {
            Serial.println("[PANCAKE] SD mount failed");
        }
    }

    void update() {
        // Reset KB poll flag each loop tick so isChange() polls fresh
        PancakeKB::resetPollFlag();
    }

    m5::board_t getBoard() { return m5::board_t::board_Unknown; }

    struct config_t {};
    config_t config() { return {}; }
};

// ---- IMU stub (BMI270 only on Cardputer ADV) ---------------
struct M5Imu_Class {
    bool getAccel(float* x, float* y, float* z) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        if (z) *z = 1.0f;  // gravity pointing down
        return false;        // no IMU present
    }
};

// ---- M5Unified_Class ---------------------------------------
struct M5Unified_Class {
    M5GFX          Display;
    M5Power_Class  Power;
    Speaker_Class  Speaker;
    M5Imu_Class    Imu;

    void update() {}
    m5::board_t getBoard() { return m5::board_t::board_Unknown; }

    struct cfg_t {};
    cfg_t config() const { return {}; }
};

// ---- Global instances (declared extern, defined in pancake_hal.cpp) --
extern M5Cardputer_Class M5Cardputer;
extern M5Unified_Class   M5;

// ---- neopixelWrite shim ------------------------------------
// porkchop calls neopixelWrite(21 /*LED_PIN*/, r, g, b).
// We remap to PANCAKE_LED_PIN via the LED_PIN macro.
// esp32-arduino provides neopixelWrite() in esp32-hal-rgb-led.h
#include <esp32-hal-rgb-led.h>
// Redirect: re-#define LED_PIN so callers automatically hit PANCAKE_LED_PIN
#ifdef LED_PIN
#undef LED_PIN
#endif
#define LED_PIN PANCAKE_LED_PIN

// ---- Keyboard redraw helper (called from Display::pushAll) -
void pancakeRedrawKeyboard();

#endif // PORKCHOP_PANCAKE
