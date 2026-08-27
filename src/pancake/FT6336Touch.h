#pragma once
// FT6336 / FT6236 / FT6436 capacitive touch driver (I2C)
// Pancake target — PORKCHOP PANCAKE port

#include <Arduino.h>
#include <Wire.h>
#include "pancake_config.h"

struct PancakeTouchPoint {
    int16_t x = -1;
    int16_t y = -1;
    bool    valid = false;
    uint8_t event = 2;   // FT6336 event: 0=press-down, 2=contact/hold (1=lift filtered).
                         // Default 2 = "hold" so consumers fall back to the release-gate.
};

class FT6336Touch {
public:
    FT6336Touch(int sda = PANCAKE_TOUCH_SDA,
                int scl = PANCAKE_TOUCH_SCL,
                int rst = PANCAKE_TOUCH_RST)
        : _sda(sda), _scl(scl), _rst(rst) {}

    bool begin() {
        if (_rst >= 0) {
            pinMode(_rst, OUTPUT);
            digitalWrite(_rst, LOW);
            delay(10);
            digitalWrite(_rst, HIGH);
            delay(50);
        }
        Wire.begin(_sda, _scl);
        uint8_t id = _readReg(0xA8);  // Vendor ID
        Serial.printf("[TOUCH] FT6336 begin sda=%d scl=%d rst=%d vendorID=0x%02X\n",
                      _sda, _scl, _rst, id);
        return (id != 0xFF && id != 0x00);
    }

    // Returns true and fills pt if a finger is currently down.
    bool getPoint(PancakeTouchPoint &pt) {
        uint8_t n = _readReg(0x02) & 0x0F;
        if (n == 0 || n > 5) { pt.valid = false; return false; }

        uint8_t xh = _readReg(0x03);
        uint8_t xl = _readReg(0x04);
        uint8_t yh = _readReg(0x05);
        uint8_t yl = _readReg(0x06);

        uint8_t event = (xh >> 6) & 0x03;  // 0=down 1=lift 2=contact
        if (event == 1) { pt.valid = false; return false; }

        pt.x = ((int16_t)(xh & 0x0F) << 8) | xl;
        pt.y = ((int16_t)(yh & 0x0F) << 8) | yl;
        pt.valid = true;
        pt.event = event;   // lets the keyboard fire each new press-down (fast taps)

        // Apply portrait rotation (rotation=2 = 180deg flip)
        if (PANCAKE_ROTATION == 2) {
            pt.x = (PANCAKE_TOUCH_W - 1) - pt.x;
            pt.y = (PANCAKE_TOUCH_H - 1) - pt.y;
        }
        return true;
    }

    // Edge-detect: returns true only on the rising edge of a touch.
    bool isTouchDown() {
        PancakeTouchPoint p;
        bool now = getPoint(p);
        bool edge = (now && !_wasDown);
        _wasDown = now;
        if (now) _last = p;
        return edge;
    }

    bool isCurrentlyDown() { return _wasDown; }
    PancakeTouchPoint lastPoint() const { return _last; }

    // Poll — returns true and fills pt on new touch event
    bool poll(PancakeTouchPoint &pt) {
        bool edge = isTouchDown();
        if (edge) pt = _last;
        return edge;
    }

private:
    int _sda, _scl, _rst;
    bool _wasDown = false;
    PancakeTouchPoint _last;

    uint8_t _readReg(uint8_t reg) {
        Wire.beginTransmission(FT6336_I2C_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return 0xFF;
        Wire.requestFrom(FT6336_I2C_ADDR, (uint8_t)1);
        return Wire.available() ? Wire.read() : 0xFF;
    }
};

// Board-agnostic touch alias (V8 aliases XPT2046Touch instead — same surface).
typedef FT6336Touch PancakeTouch;
extern PancakeTouch* pancakeTouch;
