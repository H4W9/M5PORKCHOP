#pragma once
// =============================================================
//  PancakeKeyboard — touch keyboard for the ESP32-C5 porkchop builds.
//
//  Pancake (ST7796 320x480): full QWERTY in the bottom 240 px, always visible.
//  Marauder V8 (ILI9341 240x320): a persistent 2-row nav/shortcut STRIP pinned
//    to the bottom 64 px, plus a full QWERTY OVERLAY (layer 1) that the strip's
//    ABC key toggles on/off over the lower content. Same key set as Pancake.
//
//  Keys carry a "layer": 0 = always visible (Pancake: everything; V8: strip),
//  1 = overlay (V8 only, shown when _overlayShown).
//
//  renderInto(tgt,yOff) redraws the currently-visible keys into any target
//  (a sprite) so the full screen — keyboard included — can be screenshotted on
//  panels whose framebuffer can't be read back.
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
    PKEY_ABC     = 11,  // V8 only: toggle the QWERTY overlay
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
    uint8_t layer;       // 0 = always visible, 1 = V8 overlay (shown when toggled)
};

// ============================================================
class PancakeKeyboard {
public:
    void begin(TFT_eSPI *tft, PancakeTouch *touch) {
        _tft = tft;
        _touch = touch;
        _shifted = false;
        _overlayShown = false;
        _build();
    }

    // Full redraw — call after any screen wipe
    void redraw() {
        if (!_tft) return;
#ifdef PORKCHOP_MARAUDER_V8
        _tft->fillRect(0, PANCAKE_KB_STRIP_Y, PANCAKE_SCREEN_W, PANCAKE_KB_STRIP_H, KB_BG);
        if (_overlayShown)
            _tft->fillRect(0, PANCAKE_KB_Y, PANCAKE_SCREEN_W, PANCAKE_KB_H, KB_BG);
#else
        _tft->fillRect(0, PANCAKE_KB_Y, PANCAKE_SCREEN_W, PANCAKE_KB_H, KB_BG);
#endif
        for (int i = 0; i < _n; i++)
            if (_visible(_keys[i])) _drawKeyTo(_tft, i, false, 0);
    }

    // Redraw the currently-visible keys into `tgt` (a sprite), each key's screen Y
    // shifted by yOff. Templated so the sprite's (non-virtual) draw methods resolve
    // to the sprite, not the display. Caller clears the sprite first. Used by the
    // screenshot path so the keyboard is captured even though the panel can't be
    // read back.
    template<typename G>
    void renderInto(G *tgt, int16_t yOff) {
        if (!tgt) return;
        for (int i = 0; i < _n; i++)
            if (_visible(_keys[i])) _drawKeyTo(tgt, i, false, yOff);
    }

    // Topmost screen Y the keyboard currently occupies (overlay lowers it on V8).
    int16_t screenshotTop() const {
#ifdef PORKCHOP_MARAUDER_V8
        return _overlayShown ? PANCAKE_KB_Y : PANCAKE_KB_STRIP_Y;
#else
        return PANCAKE_KB_Y;
#endif
    }

    // True when the V8 QWERTY overlay is up (content render should freeze).
    bool isOverlayActive() const {
#ifdef PORKCHOP_MARAUDER_V8
        return _overlayShown;
#else
        return false;
#endif
    }

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
        if (_lastTouchDown) return false;  // only fire on new press
        _lastTouchDown = true;

        // Reject touches outside the currently-active keyboard region(s).
#ifdef PORKCHOP_MARAUDER_V8
        bool inStrip   = tp.y >= PANCAKE_KB_STRIP_Y;
        bool inOverlay = _overlayShown && tp.y >= PANCAKE_KB_Y && tp.y < PANCAKE_KB_STRIP_Y;
        if (!inStrip && !inOverlay) return false;
#else
        if (tp.y < PANCAKE_KB_Y) return false;
#endif

        for (int i = 0; i < _n; i++) {
            PKey &k = _keys[i];
            if (!_visible(k)) continue;
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
                if (k.code == PKEY_ABC) {           // V8: toggle the QWERTY overlay
                    _overlayShown = !_overlayShown;
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
    static const int MAX_KEYS = 80;
    PKey  _keys[MAX_KEYS];
    int   _n = 0;
    bool  _shifted = false;
    bool  _overlayShown = false;
    bool  _lastTouchDown = false;
    int      _pressedKey = -1;
    uint32_t _pressedAt  = 0;

    bool _visible(const PKey &k) const {
#ifdef PORKCHOP_MARAUDER_V8
        return (k.layer == 0) || (_overlayShown && k.layer == 1);
#else
        (void)k; return true;
#endif
    }

    void _add(char ch, uint8_t code, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, code, x, y, w, h, 0, 0};
    }
    void _addS(char ch, char shiftedCh, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, PKEY_NONE, x, y, w, h, shiftedCh, 0};
    }
    // Layer-tagged variants (V8).
    void _addL(char ch, uint8_t code, uint8_t layer, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, code, x, y, w, h, 0, layer};
    }
    void _addLS(char ch, char shiftedCh, uint8_t layer, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (_n >= MAX_KEYS) return;
        _keys[_n++] = {ch, PKEY_NONE, x, y, w, h, shiftedCh, layer};
    }

#ifdef PORKCHOP_MARAUDER_V8
    void _build() {
        _n = 0;
        const int W = PANCAKE_SCREEN_W;   // 240
        const int m = PANCAKE_KB_MARGIN;  // 3

        // ---- Persistent strip (layer 0): 2 rows at the bottom ----
        const int sh = 30;                            // strip key height
        // Row A (nav): < DN UP > ENT DEL ` SCR ABC  (9 keys)
        {
            int y = PANCAKE_KB_STRIP_Y + 1;
            int n = 9;
            int w = (W - (n + 1) * m) / n;
            int x = m;
            _addL(',', PKEY_COMMA,      0, x, y, w, sh); x += w + m;
            _addL('.', PKEY_DOT,        0, x, y, w, sh); x += w + m;
            _addL(';', PKEY_SEMICOL,    0, x, y, w, sh); x += w + m;
            _addL('/', PKEY_SLASH,      0, x, y, w, sh); x += w + m;
            _addL(0,   PKEY_ENTER,      0, x, y, w, sh); x += w + m;
            _addL(0,   PKEY_BKSP,       0, x, y, w, sh); x += w + m;
            _addL('`', PKEY_BACKTICK,   0, x, y, w, sh); x += w + m;
            _addL('p', PKEY_SCREENSHOT, 0, x, y, w, sh); x += w + m;
            _addL(0,   PKEY_ABC,        0, x, y, w, sh);
        }
        // Row B (shortcuts): O B D W H F S T C G 1 2  (12 keys)
        {
            const char *r = "obdwhfstcg12";
            int y = PANCAKE_KB_STRIP_Y + 1 + 32;
            int n = 12;
            int w = (W - (n + 1) * m) / n;
            int x = m;
            for (int i = 0; i < n; i++) { _addL(r[i], PKEY_NONE, 0, x, y, w, sh); x += w + m; }
        }

        // ---- QWERTY overlay (layer 1): 5 rows over lower content ----
        const int oy = PANCAKE_KB_Y + 1;
        const int rh = PANCAKE_KB_ROW_H;              // 30
        // Row 0: digits
        { const char *r = "1234567890"; int n = 10; int w = (W - (n + 1) * m) / n; int x = m; int y = oy;
          for (int i = 0; i < n; i++) { _addL(r[i], PKEY_NONE, 1, x, y, w, rh - m); x += w + m; } }
        // Row 1: qwertyuiop
        { const char *r = "qwertyuiop"; int n = 10; int w = (W - (n + 1) * m) / n; int x = m; int y = oy + rh;
          for (int i = 0; i < n; i++) { _addL(r[i], PKEY_NONE, 1, x, y, w, rh - m); x += w + m; } }
        // Row 2: asdfghjkl
        { const char *r = "asdfghjkl"; int n = 9; int w = (W - (n + 1) * m) / n; int x = m; int y = oy + 2 * rh;
          for (int i = 0; i < n; i++) { _addL(r[i], PKEY_NONE, 1, x, y, w, rh - m); x += w + m; } }
        // Row 3: SHFT z x c v b n m DEL  (9 cells)
        { int y = oy + 3 * rh; int n = 9; int w = (W - (n + 1) * m) / n; int x = m;
          _addL(0, PKEY_SHIFT, 1, x, y, w, rh - m); x += w + m;
          const char *r = "zxcvbnm";
          for (int i = 0; i < 7; i++) { _addL(r[i], PKEY_NONE, 1, x, y, w, rh - m); x += w + m; }
          _addL(0, PKEY_BKSP, 1, x, y, w, rh - m); }
        // Row 4: -/=  SPACE(wide)  ,  .
        { int y = oy + 4 * rh; int x = m;
          int wk = 34;
          _addLS('-', '=', 1, x, y, wk, rh - m); x += wk + m;   // '-' / '='
          int wPunct = 34;
          int spW = W - m - x - 2 * (wPunct + m);
          _addL(' ', PKEY_SPACE, 1, x, y, spW, rh - m); x += spW + m;
          _addL(',', PKEY_NONE,  1, x, y, wPunct, rh - m); x += wPunct + m;
          _addL('.', PKEY_NONE,  1, x, y, wPunct, rh - m); }
    }
#else
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
        // Right-aligned nav column, shared by rows 3 & 4.
        const int wNav = 40;
        const int gtX  = W - wNav - 1;          // '>'   (rightmost)
        const int dnX  = gtX  - (wNav + m);     // 'DN'  (.)
        const int ltX  = dnX  - (wNav + m);     // '<'   (,)
        const int scrX = ltX  - (wNav + m);     // 'SCR' (p)

        // Row 3: ZXCVBNM  UP(above DN)  SHFT(right)
        {
            const char *r = "zxcvbnm";
            int n = 7;
            int w = 30;
            int y = y0 + 3*rh;
            int x = m;
            for (int i = 0; i < n; i++) { _add(r[i], PKEY_NONE, x, y, w, rh-m); x += w+m; }
            _add(';', PKEY_SEMICOL, dnX, y, wNav, rh-m);   // UP directly above DN
            _add(0,   PKEY_SHIFT,   gtX, y, wNav, rh-m);   // SHFT moved to right
        }
        // Row 4: `(back)  SPACE  SCR  <  DN  >
        {
            int y   = y0 + 4*rh;
            int wBk = 44;
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
#endif

    // Draw key i into target g (screen or sprite), with y shifted by yOff.
    // Templated so sprite targets call the sprite's own (non-virtual) methods.
    template<typename G>
    void _drawKeyTo(G *g, int i, bool pressed, int16_t yOff) {
        PKey &k = _keys[i];
        int16_t ky = k.y + yOff;
        int sc = kbShortcutClass(k.ch, k.code);
        uint16_t fill   = pressed ? KB_KEY_PRS : KB_KEY_NRM;
        uint16_t textcol= pressed ? KB_TEXT
                        : (k.code == PKEY_SCREENSHOT ? KB_CYAN
                        : (k.code == PKEY_ABC ? KB_CYAN
                        : (sc == 2 ? KB_RED : (sc == 1 ? KB_GREEN : KB_TEXT))));

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
            case PKEY_ABC:      strcpy(out, "ABC");   return;
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
