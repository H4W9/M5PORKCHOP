// pancake_hal.cpp
// Definitions for all Pancake HAL globals.
// This file is compiled exactly once, giving one definition to all
// the extern declarations in pancake_hal_impl.h.

#ifdef PORKCHOP_PANCAKE

#include "pancake_hal_impl.h"

// ---- Hardware object instances -----------------------------
// init_priority(101) ensures these construct before any translation unit's
// globals that use default priority (65535), including Display::topBar etc.
// in display.cpp whose M5Canvas constructors take &pancakeTFT as argument.
FT6336Touch     pancakeTouch     __attribute__((init_priority(101)));
PancakeKeyboard pancakeKeyboard  __attribute__((init_priority(101)));
TFT_eSPI        pancakeTFT       __attribute__((init_priority(101)));

// ---- Top-level M5 object instances -------------------------
M5Cardputer_Class M5Cardputer    __attribute__((init_priority(102)));
M5Unified_Class   M5             __attribute__((init_priority(102)));

// ---- PancakeKB state ---------------------------------------
namespace PancakeKB {
    bool    _changed  = false;
    bool    _pressed  = false;
    char    _ch       = 0;
    uint8_t _sp       = PKEY_NONE;
    bool    _polled   = false;

    void resetPollFlag() { _polled = false; }

    void poll() {
        if (_polled) return;
        _polled  = true;
        _changed = false;
        _pressed = false;
        _ch = 0;
        _sp = PKEY_NONE;
        char ch = 0;
        uint8_t sp = PKEY_NONE;
        if (pancakeKeyboard.poll(ch, sp)) {
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

// ---- Keyboard redraw helper --------------------------------
void pancakeRedrawKeyboard() {
    pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y - 1, PANCAKE_SCREEN_W, 0x528A);
    pancakeTFT.drawFastHLine(0, PANCAKE_KB_Y,     PANCAKE_SCREEN_W, 0x528A);
    pancakeKeyboard.redraw();
}

#endif // PORKCHOP_PANCAKE
