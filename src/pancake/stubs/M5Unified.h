#pragma once
// M5Unified.h stub for PORKCHOP PANCAKE
//
// Provides lightweight type stubs for files that include <M5Unified.h>
// but don't need real hardware functionality (sfx.cpp uses M5.Speaker,
// xp.h uses M5Canvas in signatures, etc.).
//
// Files that need real hardware (display, modes, main) get the full
// impl via display.h -> pancake_hal_impl.h.
//
// Key rule: NO TFT_eSPI dependency here.

#ifdef PORKCHOP_PANCAKE

#include <Arduino.h>
#include <string>
#include "../pancake_config.h"

struct lgfxFont_t {};

// Only define stub types if the real impl hasn't been loaded yet.
// pancake_hal_impl.h sets PANCAKE_HAL_IMPL_LOADED before defining
// the real versions, preventing redefinition conflicts.
#ifndef PANCAKE_HAL_IMPL_LOADED

// Forward-declare M5Canvas — enough for headers using it in signatures.
// The real definition (TFT_eSprite subclass) comes from pancake_hal_impl.h.
class M5Canvas;

namespace m5 {
    enum class board_t : uint8_t {
        board_Unknown = 0,
        board_M5CardputerADV = 99
    };
}

struct Speaker_Stub {
    void tone(uint16_t, uint32_t = 1000) {}
    void stop()             {}
    void setVolume(uint8_t) {}
    bool isEnabled()        { return false; }
};
struct Power_Stub   { int getBatteryLevel() { return 75; } };
struct Imu_Stub     {
    bool getAccel(float* x, float* y, float* z) {
        if (x) *x = 0; if (y) *y = 0; if (z) *z = 1; return false;
    }
};
struct GFX_Stub {
    void setBrightness(uint8_t b) { analogWrite(PANCAKE_TFT_BL, b); }
    void setRotation(int)         {}
    void setColorDepth(int)       {}
    void fillScreen(uint32_t)     {}
    void setTextColor(uint32_t, uint32_t = 0) {}
    void setTextSize(uint8_t)     {}
    void setTextDatum(uint8_t)    {}
    void setCursor(int, int)      {}
    void setCursor(int, int, uint8_t) {}
    void print(const char*)       {}
    void setFont(const void*)     {}
    void setFont(const lgfxFont_t*) {}
    void drawString(const char*, int, int) {}
    void fillRect(int, int, int, int, uint32_t) {}
    void drawFastHLine(int, int, int, uint32_t) {}
    void readRectRGB(int, int, int, int, uint8_t*) {}
    int  width()      { return 320; }
    int  height()     { return 240; }
    void startWrite() {}
    void endWrite()   {}
};

// Use typedef aliases matching the names porkchop uses
typedef GFX_Stub     M5GFX;
typedef Speaker_Stub Speaker_Class;
typedef Power_Stub   M5Power_Class;
typedef Imu_Stub     M5Imu_Class;

struct PancakeKeysState {
    std::string word;
    bool del   = false;
    bool enter = false;
};
struct Keyboard_Class {
    using KeysState = PancakeKeysState;
    void      update()          {}
    bool      isChange()        { return false; }
    bool      isPressed()       { return false; }
    bool      isKeyPressed(int) { return false; }
    KeysState keysState()       { return {}; }
};

// M5Cardputer_Class and M5Unified_Class stubs.
// These are LOCAL to each TU that only sees the stub — they are NOT
// the same objects as the real ones in pancake_hal.cpp.
// Files like sfx.cpp that call M5.Speaker.tone() get a local no-op stub.
struct M5Cardputer_Class {
    Keyboard_Class Keyboard;
    M5GFX          Display;
    template<typename... A> void begin(A&&...) {}
    void update() {}
    m5::board_t getBoard() { return m5::board_t::board_Unknown; }
    struct config_t {};
    config_t config() { return {}; }
};
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

// Static instances for TUs that only see the stub.
// These are separate from the real globals in pancake_hal.cpp —
// that's intentional: sfx.cpp's M5.Speaker.tone() is a no-op stub,
// which is correct (no speaker hardware on Pancake DevKit).
static M5Cardputer_Class M5Cardputer;
static M5Unified_Class   M5;

namespace fonts { static const lgfxFont_t* const Font0 = nullptr; }

#ifndef KEY_ENTER
#define KEY_ENTER     0x0D
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 0x08
#endif

// Remap LED pin
#ifdef LED_PIN
#undef LED_PIN
#endif
#define LED_PIN PANCAKE_LED_PIN

#endif // PANCAKE_HAL_IMPL_LOADED
#endif // PORKCHOP_PANCAKE
