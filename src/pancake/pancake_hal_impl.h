#pragma once
// =============================================================
//  pancake_hal_impl.h  —  Full HAL implementation
//  PORKCHOP PANCAKE (ESP32-C5-DevKitC-1 + ST7796 + FT6336)
//
//  Included only by display.h (which every porkchop .cpp includes).
//  NOT force-included — only fires after TFT_eSPI is available.
//
//  Single source of truth for all HAL types — included by stubs and display.h.
// =============================================================

#ifndef PANCAKE_HAL_IMPL_H
#define PANCAKE_HAL_IMPL_H

#ifdef PORKCHOP_PANCAKE


#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string>
#include <vector>
#include <functional>

// ---- Hardware libraries (safe here — only included by our files) --
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SD.h>
#include <WiFi.h>

#include "pancake_config.h"
#include "FT6336Touch.h"
#include "PancakeKeyboard.h"

// ---- Global hardware objects (defined in pancake_hal.cpp) ---
extern FT6336Touch     pancakeTouch;
extern PancakeKeyboard pancakeKeyboard;
extern TFT_eSPI        pancakeTFT;

// ---- M5Canvas = TFT_eSprite subclass -----------------------
class M5Canvas : public TFT_eSprite {
public:
    template<typename T>
    explicit M5Canvas(T*) : TFT_eSprite(&pancakeTFT) {}
    M5Canvas() : TFT_eSprite(&pancakeTFT) {}
    void setFont(const void*)        { setTextFont(1); }
    void setFont(const lgfxFont_t*)  { setTextFont(1); }
    using TFT_eSprite::drawString;
};

// ---- fonts namespace shim ----------------------------------
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

// ---- Keyboard shim types -----------------------------------
struct PancakeKeysState {
    std::string word;
    bool        del   = false;
    bool        enter = false;
};

namespace PancakeKB {
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
        board_M5CardputerADV = 99
    };
}

// ---- M5GFX stub --------------------------------------------
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
    int  width()      { return PANCAKE_SCREEN_W; }
    int  height()     { return PANCAKE_PORK_H; }
    void startWrite() { pancakeTFT.startWrite(); }
    void endWrite()   { pancakeTFT.endWrite(); }
};

// ---- IMU stub ----------------------------------------------
struct M5Imu_Class {
    bool getAccel(float* x, float* y, float* z) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        if (z) *z = 1.0f;
        return false;
    }
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
    void stop()             {}
    void setVolume(uint8_t) {}
    bool isEnabled()        { return false; }
};

// ---- M5Cardputer_Class -------------------------------------
struct M5Cardputer_Class {
    Keyboard_Class Keyboard;
    M5GFX          Display;

    template<typename... Args>
    void begin(Args&&...) {
        pancakeTFT.init();
        pancakeTFT.setRotation(PANCAKE_ROTATION);
        pancakeTFT.fillScreen(TFT_BLACK);
        pinMode(PANCAKE_TFT_BL, OUTPUT);
        digitalWrite(PANCAKE_TFT_BL, HIGH);

        if (!pancakeTouch.begin()) {
            Serial.println("[PANCAKE] FT6336 not found");
        }

        pancakeKeyboard.begin(&pancakeTFT, &pancakeTouch);
        pancakeKeyboard.redraw();

        pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y - 1, PANCAKE_SCREEN_W, 0x528A);
        pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y,     PANCAKE_SCREEN_W, 0x528A);

        SPI.begin(PANCAKE_TFT_CLK, PANCAKE_TFT_MISO, PANCAKE_TFT_MOSI,
                  PANCAKE_SD_CS);
        if (!SD.begin(PANCAKE_SD_CS, SPI, 25000000)) {
            Serial.println("[PANCAKE] SD mount failed");
        }
    }

    void update() { PancakeKB::resetPollFlag(); }
    m5::board_t getBoard() { return m5::board_t::board_Unknown; }

    struct config_t {};
    config_t config() { return {}; }
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

// ---- Global instances (defined in pancake_hal.cpp) ---------
extern M5Cardputer_Class M5Cardputer;
extern M5Unified_Class   M5;

// ---- neopixelWrite pin remap --------------------------------
#include <esp32-hal-rgb-led.h>
#ifdef LED_PIN
#undef LED_PIN
#endif
#define LED_PIN PANCAKE_LED_PIN

// ---- Keyboard redraw helper --------------------------------
void pancakeRedrawKeyboard();

#endif // PORKCHOP_PANCAKE
#endif // PANCAKE_HAL_IMPL_H
