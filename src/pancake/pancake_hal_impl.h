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

// lgfxFont_t — opaque font type used in setFont() signatures.
// Defined here as the single authoritative definition for the Pancake build.
struct lgfxFont_t {};

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string>
#include <vector>
#include <functional>

// ---- Hardware libraries (safe here — only included by our files) --
#include <TFT_eSPI.h>

// ---- M5GFX text datum name aliases -------------------------
// M5GFX uses descriptive names; TFT_eSPI uses *_DATUM macros.
// Map them so porkchop source compiles unchanged.
#ifndef top_left
#define top_left      TL_DATUM
#define top_center    TC_DATUM
#define top_right     TR_DATUM
#define middle_left   ML_DATUM
#define middle_center MC_DATUM
#define middle_right  MR_DATUM
#define bottom_left   BL_DATUM
#define bottom_center BC_DATUM
#define bottom_right  BR_DATUM
#endif
#include <Wire.h>
#include <SD.h>
#include <WiFi.h>

#include "pancake_config.h"
#include "FT6336Touch.h"
#include "PancakeKeyboard.h"

// ---- Global hardware objects (defined in pancake_hal.cpp) ---
extern FT6336Touch*     pancakeTouch;
extern PancakeKeyboard* pancakeKeyboard;
extern TFT_eSPI*        pancakeTFT;

// ---- M5Canvas = TFT_eSprite subclass -----------------------
class M5Canvas : public TFT_eSprite {
public:
    template<typename T>
    explicit M5Canvas(T*) : TFT_eSprite(pancakeTFT) {}
    M5Canvas() : TFT_eSprite(pancakeTFT) {}
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
        pancakeTFT->fillRect(0, 0, PANCAKE_SCREEN_W, PANCAKE_PORK_H, c);
    }
    void setTextColor(uint32_t fg, uint32_t bg = 0) {
        pancakeTFT->setTextColor(fg, bg);
    }
    void setTextSize(uint8_t s)   { pancakeTFT->setTextSize(s); }
    void setTextDatum(uint8_t d)  { pancakeTFT->setTextDatum(d); }
    void setCursor(int x, int y)  { pancakeTFT->setCursor(x, y); }
    void setCursor(int x, int y, uint8_t) { pancakeTFT->setCursor(x, y); }
    void print(const char* s)     { pancakeTFT->print(s); }
    void setFont(const void*)     {}
    void setFont(const lgfxFont_t*) {}
    void drawString(const char* s, int x, int y) {
        pancakeTFT->drawString(s, x, y);
    }
    void fillRect(int x, int y, int w, int h, uint32_t c) {
        if (y + h > PANCAKE_PORK_H) h = PANCAKE_PORK_H - y;
        if (h > 0) pancakeTFT->fillRect(x, y, w, h, c);
    }
    void drawFastHLine(int x, int y, int w, uint32_t c) {
        pancakeTFT->drawFastHLine(x, y, w, c);
    }
    void readRectRGB(int x, int y, int w, int h, uint8_t* buf) {
        pancakeTFT->readRectRGB(x, y, w, h, buf);
    }
    int  width()      { return PANCAKE_SCREEN_W; }
    int  height()     { return PANCAKE_PORK_H; }
    void startWrite() { pancakeTFT->startWrite(); }
    void endWrite()   { pancakeTFT->endWrite(); }
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

// ---- RTC stub (no hardware RTC on Pancake DevKit) ----------
// Returns zeroed structs so year < 2024 check fails everywhere,
// causing callers to fall through to their existing time() fallback.
struct PancakeRtcDate { uint16_t year = 0; uint8_t month = 0, date = 0; };
struct PancakeRtcTime { uint8_t hours = 0, minutes = 0, seconds = 0; };
struct PancakeRtcDateTime { PancakeRtcDate date; PancakeRtcTime time; };
struct PancakeRtc_Class {
    PancakeRtcDateTime getDateTime() { return {}; }
};

// ---- Power — MAX17048 fuel gauge (I2C 0x36) ----------------
// Pancake hardware has a MAX17048 on the same I2C bus as the touch
// controller (SDA=PANCAKE_TOUCH_SDA, SCL=PANCAKE_TOUCH_SCL).
//
// Registers:
//   0x02 VCELL  — battery voltage, 1 LSB = 78.125 µV (raw >> 4 * 1.25 mV)
//   0x04 SOC    — state of charge, MSB = whole %, LSB = 1/256 %
//   0x08 STATUS — bit 0 = Vreset alert, etc.
//
// Charging detection: MAX17048 doesn't have a charge pin, but VBUS
// presence (USB connected) implies charging.  We detect VBUS by
// comparing cell voltage to a threshold: >4.05 V and rising = charging.
// For simplicity, report charge_unknown and let the caller decide.

#define MAX17048_ADDR   0x36
#define MAX17048_VCELL  0x02
#define MAX17048_SOC    0x04

namespace m5 {
    struct Power_Class {
        enum class is_charging_t : uint8_t {
            is_discharging = 0,
            is_charging    = 1,
            charge_unknown = 2
        };
    };
}

struct M5Power_Class {
private:
    static uint16_t _readReg16(uint8_t reg) {
        Wire.beginTransmission(MAX17048_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return 0xFFFF;
        Wire.requestFrom((uint8_t)MAX17048_ADDR, (uint8_t)2);
        if (Wire.available() < 2) return 0xFFFF;
        uint16_t val = (uint16_t)Wire.read() << 8;
        val |= Wire.read();
        return val;
    }

public:
    // Returns battery voltage in millivolts (e.g. 3750 = 3.75 V)
    int getBatteryVoltage() {
        uint16_t raw = _readReg16(MAX17048_VCELL);
        if (raw == 0xFFFF) return 3700;  // fallback if not present
        // VCELL: each LSB = 78.125 µV → mV = raw * 78.125 / 1000
        // Simplified integer: raw * 5 / 64  (close enough, <0.1% error)
        return (int)((uint32_t)raw * 5 / 64);
    }

    // Returns state of charge 0-100 %
    int getBatteryLevel() {
        uint16_t raw = _readReg16(MAX17048_SOC);
        if (raw == 0xFFFF) return 75;   // fallback
        int pct = (raw >> 8) & 0xFF;    // integer percent in MSB
        if (pct > 100) pct = 100;
        return pct;
    }

    // MAX17048 doesn't expose a charge pin.
    // Return charge_unknown — callers handle this gracefully.
    m5::Power_Class::is_charging_t isCharging() {
        return m5::Power_Class::is_charging_t::charge_unknown;
    }

    // VBUS voltage not available from MAX17048.
    int getVBUSVoltage() { return 0; }
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
        Serial.println("[HAL] begin() entry");
        Serial.flush();

        // Keep backlight OFF until display is initialized to prevent flicker
        pinMode(PANCAKE_TFT_BL, OUTPUT);
        analogWrite(PANCAKE_TFT_BL, 0);

        Serial.println("[HAL] Calling pancakeTFT->init()...");
        Serial.flush();
        pancakeTFT->init();
        Serial.println("[HAL] pancakeTFT->init() done");
        Serial.flush();

        pancakeTFT->setRotation(PANCAKE_ROTATION);
        pancakeTFT->fillScreen(TFT_BLACK);

        // Now turn on backlight — display is ready
        analogWrite(PANCAKE_TFT_BL, 255);
        Serial.println("[HAL] TFT init complete, BL on");
        Serial.flush();

        if (!pancakeTouch->begin()) {
            Serial.println("[PANCAKE] FT6336 not found");
        }

        pancakeKeyboard->begin(pancakeTFT, pancakeTouch);
        pancakeKeyboard->redraw();

        pancakeTFT->drawFastHLine(0, PANCAKE_KB_Y - 1, PANCAKE_SCREEN_W, 0x528A);
        pancakeTFT->drawFastHLine(0, PANCAKE_KB_Y,     PANCAKE_SCREEN_W, 0x528A);

        // SD is mounted by Config::init() (dedicated SPI bus, correct pins,
        // multi-speed retry). Do not SD.begin() here.
    }

    void update() { PancakeKB::resetPollFlag(); }
    m5::board_t getBoard() { return m5::board_t::board_Unknown; }

    struct config_t {};
    config_t config() { return {}; }
};

// ---- M5Unified_Class ---------------------------------------
struct M5Unified_Class {
    M5GFX              Display;
    M5Power_Class      Power;
    Speaker_Class      Speaker;
    M5Imu_Class        Imu;
    PancakeRtc_Class   Rtc;

    void update() {}
    m5::board_t getBoard() { return m5::board_t::board_Unknown; }

    struct cfg_t {};
    cfg_t config() const { return {}; }
};

// ---- Global instances (defined in pancake_hal.cpp) ---------
extern M5Cardputer_Class* pancakeM5Cardputer;
extern M5Unified_Class*   pancakeM5;
// Dereference macros so existing code using M5Cardputer.x and M5.x still works
#define M5Cardputer (*pancakeM5Cardputer)
#define M5          (*pancakeM5)

// ---- neopixelWrite pin remap --------------------------------
#include <esp32-hal-rgb-led.h>
#ifdef LED_PIN
#undef LED_PIN
#endif
#define LED_PIN PANCAKE_LED_PIN

// ---- Keyboard redraw helper --------------------------------
void pancakeHalInit();        // Call from setup() before M5Cardputer.begin()
void pancakeRedrawKeyboard();

#endif // PORKCHOP_PANCAKE
#endif // PANCAKE_HAL_IMPL_H
