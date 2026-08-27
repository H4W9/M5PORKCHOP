#pragma once
// =============================================================
//  PancakeKeyboard — touch QWERTY keyboard for the ESP32-C5 porkchop builds.
//
//  Same layout on both boards; only the geometry (from pancake_config.h) differs:
//    Pancake (ST7796 320x480): bottom 240 px.
//    Marauder V8 (ILI9341 240x320): bottom 185 px (content pane is only 135 px).
//  Hardcoded key widths scale with PANCAKE_SCREEN_W so the identical layout fits
//  both 320 px and 240 px (at 320 the values are unchanged).
//
//  renderInto(tgt,yOff) redraws the keys into any target (a sprite) so the full
//  screen — keyboard included — can be screenshotted on panels whose framebuffer
//  can't be read back.
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
    PKEY_SCREENSHOT = 10, // SCR — emits 'p' (screenshot) but labelled SCR, cyan
};

// ---- Colour palette ----------------------------------------
static const uint16_t KB_BG      = TFT_BLACK;
static const uint16_t KB_KEY_NRM = 0x2104;   // dark grey
static const uint16_t KB_KEY_PRS = 0x8410;   // pressed: lighter grey (more visible)
static const uint16_t KB_BORDER  = 0x528A;
static const uint16_t KB_TEXT    = TFT_WHITE;
static const uint16_t KB_RED     = TFT_RED;
static const uint16_t KB_GREEN   = TFT_GREEN;
static const uint16_t KB_CYAN    = TFT_CYAN;

// How long a tapped key stays highlighted (ms). Non-blocking — cleared on a
// later poll(), so it never slows down rapid successive taps.
static const uint32_t KB_PRESS_MS = 90;

// ---- Shortcut classification --------------------------------
// Returns 0=normal 1=green(nav) 2=red(attack) 3=cyan(screenshot)
static inline int kbShortcutClass(char ch, uint8_t code) {
    if (code == PKEY_ENTER  || code == PKEY_BKSP    ||
        code == PKEY_BACKTICK|| code == PKEY_SEMICOL ||
        code == PKEY_DOT    || code == PKEY_COMMA   ||
        code == PKEY_SLASH  || code == PKEY_SPACE)  return 1;
    if (!ch) return 0;
    char c = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
    if (c == 'P') return 3;   // screenshot — same key the SCR button emits
    if (c == 'O') return 2;
    if (c == 'B') return 2;
    if (c == 'D' || c == 'W' || c == 'H' || c == 'F' ||
        c == 'S' || c == 'T' || c == 'C' || c == 'G') return 1;
#ifdef PANCAKE_BUZZER_ENABLED
    if (c == 'A') return 1;   // audio toggle — only wired up on buzzer-equipped hardware
#endif
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
    void begin(TFT_eSPI *tft, PancakeTouch *touch) {
        _tft = tft;
        _touch = touch;
        _shifted = false;
        _build();
    }

    // Full redraw — call after any screen wipe
    void redraw() {
        if (!_tft) return;
        _tft->fillRect(0, PANCAKE_KB_Y, PANCAKE_SCREEN_W, PANCAKE_KB_H, KB_BG);
        for (int i = 0; i < _n; i++) _drawKeyTo(_tft, i, false, 0);
    }

    // Redraw the keys into `tgt` (a sprite), each key's screen Y shifted by yOff.
    // Templated so the sprite's (non-virtual) draw methods resolve to the sprite,
    // not the display. Caller clears the sprite first. Used by the screenshot path.
    template<typename G>
    void renderInto(G *tgt, int16_t yOff) {
        if (!tgt) return;
        for (int i = 0; i < _n; i++) _drawKeyTo(tgt, i, false, yOff);
    }

    // Topmost screen Y the keyboard occupies (for the screenshot compositor).
    int16_t screenshotTop() const { return PANCAKE_KB_Y; }

    // Poll for a key tap. Returns true when a key was pressed.
    bool poll(char &ch, uint8_t &sp) {
        ch = 0; sp = PKEY_NONE;
        if (!_touch) return false;

        // Clear an expired momentary highlight without blocking (fast taps).
        if (_pressedKey >= 0 && (millis() - _pressedAt) >= KB_PRESS_MS) {
            _drawKeyTo(_tft, _pressedKey, false, 0);
            _pressedKey = -1;
        }

        PancakeTouchPoint tp;
        bool touched = _touch->getPoint(tp) && tp.valid;
        if (!touched) { _lastTouchDown = false; return false; }
        // Fire on a new press. Normally that means the release-gate has cleared
        // (a prior frame saw no finger), but the FT6336 also flags a fresh
        // press-down (event 0) — honor that so a rapid re-tap registers even if
        // the poll loop never sampled the brief lift between taps.
        bool freshPress = (tp.event == 0);
        if (_lastTouchDown && !freshPress) return false;  // holding, not a new tap
        _lastTouchDown = true;

        if (tp.y < PANCAKE_KB_Y) return false;

        for (int i = 0; i < _n; i++) {
            PKey &k = _keys[i];
            if (tp.x >= k.x && tp.x < k.x + k.w &&
                tp.y >= k.y && tp.y < k.y + k.h)
            {
                if (_pressedKey >= 0 && _pressedKey != i) _drawKeyTo(_tft, _pressedKey, false, 0);
                _drawKeyTo(_tft, i, true, 0);
                _pressedKey = i;
                _pressedAt  = millis();

                if (k.code == PKEY_SHIFT) {
                    _shifted = !_shifted;
                    redraw();
                    _pressedKey = -1;
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
    PancakeTouch *_touch = nullptr;
    static const int MAX_KEYS = 70;
    PKey  _keys[MAX_KEYS];
    int   _n = 0;
    bool  _shifted = false;
    bool  _lastTouchDown = false;
    int      _pressedKey = -1;
    uint32_t _pressedAt  = 0;

    void _add(char ch, uint8_t code, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, code, x, y, w, h, 0};
    }
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
        // Fixed widths scale with screen width so the same layout fits 320 & 240.
        const int wDel = W * 50 / 320;
        const int wE   = W * 62 / 320;
        const int wNav = W * 40 / 320;
        const int wBk  = W * 44 / 320;

        // Row 0: 1 2 3 4 5 6 7 8 9 0  [-/=]  DEL(wide)
        {
            const char *r = "1234567890";
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
            int w  = (W - (n+2)*m - wE) / n;
            int y  = y0 + 2*rh;
            int x  = m;
            for (int i = 0; i < n; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _add(0, PKEY_ENTER, x, y, W-x-1, rh-m);
        }
        // Right-aligned nav column, shared by rows 3 & 4.
        const int gtX  = W - wNav - 1;          // '>'   (rightmost)
        const int dnX  = gtX  - (wNav + m);     // 'DN'  (.)
        const int ltX  = dnX  - (wNav + m);     // '<'   (,)
        const int scrX = ltX  - (wNav + m);     // 'SCR' (p)

        // Row 3: ZXCVBNM  UP(above DN)  SHFT(right)
        {
            const char *r = "zxcvbnm";
            int n = 7;
            int w = (dnX - 8*m) / 7;              // fill up to the UP key (fits 320 & 240)
            int y = y0 + 3*rh;
            int x = m;
            for (int i = 0; i < n; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _add(';', PKEY_SEMICOL, dnX, y, wNav, rh-m);   // UP directly above DN
            _add(0,   PKEY_SHIFT,   gtX, y, wNav, rh-m);   // SHFT moved to right
        }
        // Row 4: `(back)  SPACE  SCR  <  DN  >
        {
            int y   = y0 + 4*rh;
            _add('`', PKEY_BACKTICK, m, y, wBk, rh-m);
            int spX = m + wBk + m;
            int spW = scrX - m - spX;
            _add(' ', PKEY_SPACE,     spX,  y, spW,  rh-m);
            _add('p', PKEY_SCREENSHOT, scrX, y, wNav, rh-m); // SCR (cyan; emits 'p')
            _add(',', PKEY_COMMA,     ltX,  y, wNav, rh-m); // <
            _add('.', PKEY_DOT,       dnX,  y, wNav, rh-m); // DN
            _add('/', PKEY_SLASH,     gtX,  y, wNav, rh-m); // >
        }
    }

    // Draw key i into target g (screen or sprite), with y shifted by yOff.
    // Templated so sprite targets call the sprite's own (non-virtual) methods.
    template<typename G>
    void _drawKeyTo(G *g, int i, bool pressed, int16_t yOff) {
        PKey &k = _keys[i];
        int16_t ky = k.y + yOff;
        int sc = kbShortcutClass(k.ch, k.code);
        uint16_t fill   = pressed ? KB_KEY_PRS : KB_KEY_NRM;
        uint16_t textcol= pressed ? KB_TEXT
                        : (sc == 3 ? KB_CYAN
                        : (sc == 2 ? KB_RED : (sc == 1 ? KB_GREEN : KB_TEXT)));

        g->fillRect(k.x, ky, k.w, k.h, fill);
        g->drawRect(k.x, ky, k.w, k.h, KB_BORDER);

        char label[8];
        _label(k, label);

        g->setTextColor(textcol, fill);
        g->setTextSize(1);
        g->setTextFont(2);  // 16pt

        int tw = g->textWidth(label);
        int th = g->fontHeight();
        g->setCursor(k.x + (k.w - tw)/2, ky + (k.h - th)/2);
        g->print(label);
    }

    void _label(const PKey &k, char *out) {
        switch (k.code) {
            case PKEY_BKSP:     strcpy(out, "DEL");   return;
            case PKEY_ENTER:    strcpy(out, "ENT");   return;
            case PKEY_SHIFT:    strcpy(out, _shifted ? "SHFT" : "shft"); return;
            case PKEY_SPACE:    strcpy(out, "SPC");   return;
            case PKEY_BACKTICK: strcpy(out, "`/BK");  return;
            case PKEY_SEMICOL:  strcpy(out, ";/UP");  return;
            case PKEY_DOT:      strcpy(out, "./DN");  return;
            case PKEY_COMMA:    strcpy(out, ",/<");   return;
            case PKEY_SLASH:    strcpy(out, "//>"); return;
            case PKEY_SCREENSHOT: strcpy(out, "SCR"); return;
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
