// pancake_hal.cpp
// Definitions for all Pancake HAL globals.
// All hardware objects are heap-allocated in pancakeHalInit() which must be
// called from setup() before any HAL use. This avoids static-init crashes on
// ESP32-C5 where the MSPI timing code consumes heap before __init_array runs.

#ifdef PORKCHOP_PANCAKE

#include "pancake_hal_impl.h"

// ---- Hardware object pointers (null until pancakeHalInit()) ----------------
PancakeTouch*    pancakeTouch    = nullptr;   // FT6336 (Pancake) or XPT2046 (V8)
PancakeKeyboard* pancakeKeyboard = nullptr;
TFT_eSPI*        pancakeTFT      = nullptr;

// ---- Top-level M5 object pointers ------------------------------------------
M5Cardputer_Class* pancakeM5Cardputer = nullptr;
M5Unified_Class*   pancakeM5          = nullptr;

// ---- Convenience references exposed via macros in pancake_hal_impl.h -------
// (M5Cardputer and M5 are #defined to dereference these pointers)

// ---- PancakeKB state --------------------------------------------------------
namespace PancakeKB {
    bool    _changed  = false;
    bool    _pressed  = false;
    char    _ch       = 0;
    uint8_t _sp       = PKEY_NONE;
    bool    _polled   = false;

    void resetPollFlag() { _polled = false; }

    void poll() {
        if (_polled || !pancakeKeyboard) return;
        _polled  = true;
        _changed = false;
        _pressed = false;
        _ch = 0;
        _sp = PKEY_NONE;
        char ch = 0;
        uint8_t sp = PKEY_NONE;
        if (pancakeKeyboard->poll(ch, sp)) {
            _changed = true;
            _pressed = true;
            _ch = ch;
            _sp = sp;
        }
    }

    bool isChange()  { poll(); return _changed; }
    bool isPressed() { poll(); return _pressed; }

    bool isKeyPressed(int key) {
        poll();
        if (!_pressed) return false;
        if (key == KEY_ENTER)     return (_sp == PKEY_ENTER);
        if (key == KEY_BACKSPACE) return (_sp == PKEY_BKSP);
        if (key == (int)'`')  return (_sp == PKEY_BACKTICK || _ch == '`');
        if (key == (int)';')  return (_sp == PKEY_SEMICOL  || _ch == ';');
        if (key == (int)'.')  return (_sp == PKEY_DOT      || _ch == '.');
        if (key == (int)',')  return (_sp == PKEY_COMMA    || _ch == ',');
        if (key == (int)'/')  return (_sp == PKEY_SLASH    || _ch == '/');
        if (key == (int)' ')  return (_sp == PKEY_SPACE    || _ch == ' ');
        if (_ch) {
            char kl = (key >= 'A' && key <= 'Z') ? (char)(key + 32) : (char)key;
            char cl = (_ch >= 'A' && _ch <= 'Z') ? (char)(_ch + 32) : _ch;
            return (kl == cl);
        }
        return false;
    }

    PancakeKeysState keysState() {
        poll();
        PancakeKeysState s;
        s.del   = (_sp == PKEY_BKSP);
        s.enter = (_sp == PKEY_ENTER);
        if (_ch && !s.del && !s.enter) s.word = std::string(1, _ch);
        return s;
    }
}  // namespace PancakeKB

// ---- Keyboard redraw helper -------------------------------------------------
void pancakeRedrawKeyboard() {
    if (!pancakeTFT || !pancakeKeyboard) return;
    pancakeTFT->drawFastHLine(0, PANCAKE_KB_Y - 1, PANCAKE_SCREEN_W, 0x528A);
    pancakeTFT->drawFastHLine(0, PANCAKE_KB_Y,     PANCAKE_SCREEN_W, 0x528A);
    pancakeKeyboard->redraw();
}

// ---- One-time hardware init — call from setup() BEFORE M5Cardputer.begin() -
void pancakeHalInit() {
    pancakeTFT      = new TFT_eSPI();
    pancakeTouch    = new PancakeTouch();   // FT6336 (Pancake) / XPT2046 (V8)
    pancakeKeyboard = new PancakeKeyboard();
    pancakeM5Cardputer = new M5Cardputer_Class();
    pancakeM5          = new M5Unified_Class();
}

#endif // PORKCHOP_PANCAKE
