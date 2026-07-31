#pragma once
// =============================================================
//  PancakeKeyboard — touch QWERTY keyboard for PORKCHOP PANCAKE
//
//  Rendered in the bottom 240 px of the 320x480 portrait screen.
//
//  Keys are coloured:
//    RED   = attack/chaos shortcuts  (O B)
//    GREEN = mode/nav shortcuts      (D W H F S T C 1 2 G
//                                     ENTER BKSP ; . , / ` SPACE)
//    WHITE = normal keys
//
//  API used by PancakeInput shim:
//    begin(tft*)  — call once at startup, draws keyboard
//    redraw()     — re-render full keyboard (call after screen clears)
//    poll(ch, sp) — returns true + fills ch/sp when a key is tapped
// =============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pancake_config.h"

// ---- Special key codes (ch==0 when code is set) ------------
enum PancakeSpecial : uint8_t {
    PKEY_NONE    = 0,
    PKEY_ENTER   = 1,
    PKEY_BKSP    = 2,
    PKEY_SHIFT   = 3,
    PKEY_SPACE   = 4,
    PKEY_BACKTICK= 5,   // ` — back/cancel in porkchop
    PKEY_SEMICOL = 6,   // ; — scroll up / nav up
    PKEY_DOT     = 7,   // . — scroll down / nav down
    PKEY_COMMA   = 8,   // , — spectrum pan left
    PKEY_SLASH   = 9,   // / — spectrum pan right
};

// ---- Colour palette ----------------------------------------
static const uint16_t KB_BG      = TFT_BLACK;
static const uint16_t KB_KEY_NRM = 0x2104;   // dark grey
static const uint16_t KB_KEY_PRS = 0x4208;   // pressed (lighter)
static const uint16_t KB_BORDER  = 0x528A;
static const uint16_t KB_TEXT    = TFT_WHITE;
static const uint16_t KB_RED     = TFT_RED;
static const uint16_t KB_GREEN   = TFT_GREEN;

// ---- Shortcut classification --------------------------------
// Returns 0=normal 1=green(nav) 2=red(attack)
static inline int kbShortcutClass(char ch, uint8_t code) {
    if (code == PKEY_ENTER  || code == PKEY_BKSP    ||
        code == PKEY_BACKTICK|| code == PKEY_SEMICOL ||
        code == PKEY_DOT    || code == PKEY_COMMA   ||
        code == PKEY_SLASH  || code == PKEY_SPACE)  return 1;
    if (!ch) return 0;
    char c = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
    if (c == 'O') return 2;
    if (c == 'B') return 2;
    if (c == 'D' || c == 'W' || c == 'H' || c == 'F' ||
        c == 'S' || c == 'T' || c == 'C' || c == 'G') return 1;
    if (c == '1' || c == '2') return 1;
    return 0;
}

// ---- Key descriptor ----------------------------------------
struct PKey {
    char    ch;
    uint8_t code;
    int16_t x, y, w, h;
    char    shiftedCh;   // char emitted/shown when shifted (0 = fall back to toupper(ch))
};

// ============================================================
class PancakeKeyboard {
public:
    void begin(TFT_eSPI *tft, FT6336Touch *touch) {
        _tft = tft;
        _touch = touch;
        _shifted = false;
        _build();
    }

    // Full redraw — call after any screen wipe
    void redraw() {
        if (!_tft) return;
        _tft->fillRect(0, PANCAKE_KB_Y, PANCAKE_SCREEN_W, PANCAKE_KB_H, KB_BG);
        for (int i = 0; i < _n; i++) _drawKey(i, false);
    }

    // Poll for a key tap. Returns true when a key was pressed.
    // ch  = character (0 if special-only key)
    // sp  = PancakeSpecial code (PKEY_NONE if regular char)
    bool poll(char &ch, uint8_t &sp) {
        ch = 0; sp = PKEY_NONE;
        if (!_touch) return false;

        // Read the CURRENT touch directly. (Do not use isCurrentlyDown() —
        // that returns _wasDown, which only isTouchDown() updates and nothing
        // calls, so it was always false and no key ever registered.)
        PancakeTouchPoint tp;
        bool touched = _touch->getPoint(tp) && tp.valid;

        if (!touched) {
            _lastTouchDown = false;
            return false;
        }
        if (_lastTouchDown) return false;  // only fire on new press
        _lastTouchDown = true;

        if (tp.y < PANCAKE_KB_Y) return false;

        for (int i = 0; i < _n; i++) {
            PKey &k = _keys[i];
            if (tp.x >= k.x && tp.x < k.x + k.w &&
                tp.y >= k.y && tp.y < k.y + k.h)
            {
                _drawKey(i, true);
                delay(35);
                _drawKey(i, false);

                if (k.code == PKEY_SHIFT) {
                    _shifted = !_shifted;
                    redraw();
                    return false;
                }

                sp = k.code;
                if (k.ch) {
                    if (_shifted) ch = k.shiftedCh ? k.shiftedCh : (char)toupper(k.ch);
                    else          ch = k.ch;
                }
                return true;
            }
        }
        return false;
    }

private:
    TFT_eSPI     *_tft   = nullptr;
    FT6336Touch  *_touch = nullptr;
    static const int MAX_KEYS = 70;
    PKey  _keys[MAX_KEYS];
    int   _n = 0;
    bool  _shifted = false;
    bool  _lastTouchDown = false;

    void _add(char ch, uint8_t code, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, code, x, y, w, h, 0};
    }
    // Key with a shift alternate char (e.g. '-' / '=').
    void _addS(char ch, char shiftedCh, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, PKEY_NONE, x, y, w, h, shiftedCh};
    }

    void _build() {
        _n = 0;
        const int W  = PANCAKE_SCREEN_W;
        const int y0 = PANCAKE_KB_Y + 4;   // 4 px top padding
        const int rh = PANCAKE_KB_ROW_H;
        const int m  = PANCAKE_KB_MARGIN;

        // Row 0: 1 2 3 4 5 6 7 8 9 0  [-/=]  DEL(wide)
        {
            const char *r = "1234567890";
            int wDel = 50;                               // big DEL
            int w = (W - wDel - 12*m) / 11;              // 10 digits + 1 combined key
            int y = y0;
            int x = m;
            for (int i = 0; i < 10; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _addS('-', '=', x, y, w, rh-m); x += w+m;    // '-' normal, '=' when shifted
            _add(0, PKEY_BKSP, x, y, W-x-1, rh-m);       // DEL takes the rest
        }
        // Row 1: QWERTYUIOP
        {
            const char *r = "qwertyuiop";
            int n = 10;
            int w = (W - (n+1)*m) / n;
            int y = y0 + rh;
            for (int i = 0; i < n; i++)
                _add(r[i], PKEY_NONE, m + i*(w+m), y, w, rh-m);
        }
        // Row 2: ASDFGHJKL  ENTER(wide)
        {
            const char *r = "asdfghjkl";
            int n  = 9;
            int wE = 62;
            int w  = (W - (n+2)*m - wE) / n;
            int y  = y0 + 2*rh;
            int x  = m;
            for (int i = 0; i < n; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _add(0, PKEY_ENTER, x, y, W-x-1, rh-m);
        }
        // Row 3: SHIFT  ZXCVBNM  ;(UP)
        {
            const char *r = "zxcvbnm";
            int n   = 7;
            int wSh = 44, wUp = 44;
            int w   = (W - 2*m - wSh - wUp - (n+1)*m) / n;
            int y   = y0 + 3*rh;
            _add(0, PKEY_SHIFT, m, y, wSh, rh-m);
            int x = m + wSh + m;
            for (int i = 0; i < n; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _add(';', PKEY_SEMICOL, W-wUp-1, y, wUp, rh-m);   // UP at right edge
        }
        // Row 4: `(back)  SPACE(small)  ,(<)  .(DN)  /(>)  p(shot)
        {
            int y    = y0 + 4*rh;
            int wBk  = 44, wNav = 40, wP = 40;
            int x    = m;
            _add('`', PKEY_BACKTICK, x, y, wBk, rh-m); x += wBk + m;
            int rightW = 3*wNav + wP + 4*m;              // , . / p + gaps
            int spW    = W - x - rightW - 1;
            _add(' ', PKEY_SPACE,  x, y, spW,  rh-m); x += spW  + m;
            _add(',', PKEY_COMMA,  x, y, wNav, rh-m); x += wNav + m;
            _add('.', PKEY_DOT,    x, y, wNav, rh-m); x += wNav + m;
            _add('/', PKEY_SLASH,  x, y, wNav, rh-m);
            _add('p', PKEY_NONE,   W-wP-1, y, wP, rh-m);
        }
    }

    void _drawKey(int i, bool pressed) {
        PKey &k = _keys[i];
        int sc = kbShortcutClass(k.ch, k.code);
        uint16_t fill   = pressed ? KB_KEY_PRS : KB_KEY_NRM;
        uint16_t textcol= pressed ? KB_TEXT
                        : (sc == 2 ? KB_RED : (sc == 1 ? KB_GREEN : KB_TEXT));

        _tft->fillRect(k.x, k.y, k.w, k.h, fill);
        _tft->drawRect(k.x, k.y, k.w, k.h, KB_BORDER);

        char label[8];
        _label(k, label);

        _tft->setTextColor(textcol, fill);
        _tft->setTextSize(1);
        _tft->setTextFont(2);  // 16pt

        int tw = _tft->textWidth(label);
        int th = _tft->fontHeight();
        _tft->setCursor(k.x + (k.w - tw)/2, k.y + (k.h - th)/2);
        _tft->print(label);
    }

    void _label(const PKey &k, char *out) {
        switch (k.code) {
            case PKEY_BKSP:     strcpy(out, "DEL");   return;
            case PKEY_ENTER:    strcpy(out, "ENT");   return;
            case PKEY_SHIFT:    strcpy(out, _shifted ? "SHF" : "shf"); return;
            case PKEY_SPACE:    strcpy(out, "SPC");   return;
            case PKEY_BACKTICK: strcpy(out, "`/BK");  return;
            case PKEY_SEMICOL:  strcpy(out, ";/UP");  return;
            case PKEY_DOT:      strcpy(out, "./DN");  return;
            case PKEY_COMMA:    strcpy(out, ",/<");   return;
            case PKEY_SLASH:    strcpy(out, "//>"); return;
            default: break;
        }
        if (k.ch) {
            char c;
            if (_shifted) c = k.shiftedCh ? k.shiftedCh : (char)toupper(k.ch);
            else          c = k.ch;
            out[0] = c; out[1] = '\0';
        } else {
            strcpy(out, "?");
        }
    }
};

extern PancakeKeyboard* pancakeKeyboard;
