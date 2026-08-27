#pragma once
// XPT2046 resistive touch driver for Marauder V8.
// Wraps TFT_eSPI's built-in XPT2046 support (TOUCH_CS from the User_Setup) so it
// exposes the same getPoint()/PancakeTouchPoint surface as FT6336Touch, letting
// PancakeKeyboard stay board-agnostic. Resistive panels need calibration; the
// 5-value TFT_eSPI cal array is persisted in SPIFFS ("/touchcal.prf").

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pancake_config.h"
#include "../core/spiffs_prefs.h"

struct PancakeTouchPoint {
    int16_t x = -1;
    int16_t y = -1;
    bool    valid = false;
    uint8_t event = 2;   // resistive panel has no press-down event; always "hold"
                         // so the keyboard uses its release-gate (unchanged on V8).
};

extern TFT_eSPI* pancakeTFT;   // shared display + touch SPI instance

class XPT2046Touch {
public:
    bool begin() {
        if (loadCal(_cal)) {
            pancakeTFT->setTouch(_cal);
            _calibrated = true;
            Serial.println("[TOUCH] XPT2046 begin: calibration loaded");
        } else {
            Serial.println("[TOUCH] XPT2046 begin: NO calibration — run touch calibrate");
        }
        return true;
    }

    // Returns true and fills pt if the panel is currently pressed.
    bool getPoint(PancakeTouchPoint &pt) {
        uint16_t x = 0, y = 0;
        if (!pancakeTFT->getTouch(&x, &y, TOUCH_THRESHOLD)) {
            pt.valid = false;
            return false;
        }
        pt.x = (int16_t)x;
        pt.y = (int16_t)y;
        pt.valid = true;
        return true;
    }

    bool isCurrentlyDown() { PancakeTouchPoint p; return getPoint(p); }
    bool isCalibrated() const { return _calibrated; }

    // Interactive calibration: draw the four corner targets, store the result.
    // Runs directly on the TFT (call before sprites take over the screen).
    void calibrate() {
        pancakeTFT->fillScreen(TFT_BLACK);
        pancakeTFT->setTextColor(TFT_WHITE, TFT_BLACK);
        pancakeTFT->setTextSize(1);
        pancakeTFT->setTextDatum(TL_DATUM);
        pancakeTFT->drawString("Touch the corner arrows", 10, 10);
        pancakeTFT->calibrateTouch(_cal, TFT_MAGENTA, TFT_BLACK, 15);
        pancakeTFT->setTouch(_cal);
        _calibrated = true;
        saveCal(_cal);
        Serial.println("[TOUCH] XPT2046: calibration saved");
    }

private:
    static const uint16_t TOUCH_THRESHOLD = 600;
    uint16_t _cal[5] = {0};
    bool _calibrated = false;

    static bool loadCal(uint16_t* cal) {
        SpiffsPrefs p; p.begin("touchcal", true);
        bool ok = p.getUInt("valid", 0) == 1;
        if (ok) {
            for (int i = 0; i < 5; i++) {
                char k[6]; snprintf(k, sizeof k, "c%d", i);
                cal[i] = p.getUShort(k, 0);
            }
        }
        p.end();
        return ok;
    }

    static void saveCal(const uint16_t* cal) {
        SpiffsPrefs p; p.begin("touchcal");
        for (int i = 0; i < 5; i++) {
            char k[6]; snprintf(k, sizeof k, "c%d", i);
            p.putUShort(k, cal[i]);
        }
        p.putUInt("valid", 1);
        p.end();   // flushes to SPIFFS
    }
};

typedef XPT2046Touch PancakeTouch;
extern PancakeTouch* pancakeTouch;
