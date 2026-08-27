#pragma once

#include <cstdint>

// ============================================================================
// Timezone table (POSIX TZ strings) — DST-aware, worldwide.
// ----------------------------------------------------------------------------
// Porkchop stores the clock in UTC (see rtc_ds3231 / GPS / NTP sync) and applies
// the user's zone only for display, via the C library's TZ machinery:
//   Timezones::apply(index) -> setenv("TZ", posix, 1) + tzset()
// after which localtime_r() yields correct local wall time, including DST
// transitions (US, EU, and inverted southern-hemisphere rules).
//
// This list is byte-for-byte the same as the ASCII-Aquarium Pancake firmware's
// table (same names, same POSIX strings, same order) so the two firmwares on
// this shared hardware pick the SAME zone by the SAME index and always agree.
// kDefaultIndex (Central) also matches Aquarium's default.
// ============================================================================
namespace Timezones {
    struct Option {
        const char* name;   // short label shown in the settings picker
        const char* posix;  // POSIX TZ string handed to setenv("TZ", ...)
    };

    extern const Option kOptions[];
    static constexpr int kCount = 30;            // entries in kOptions (asserted in .cpp)

    static constexpr uint8_t kDefaultIndex = 5;  // "Central" (matches Aquarium)

    const char* posixFor(uint8_t index);  // clamped to a valid entry
    const char* nameFor(uint8_t index);   // clamped to a valid entry

    // Point the C library's local-time conversion at this zone. Call at boot
    // (from the loaded config) and whenever the setting changes.
    void apply(uint8_t index);
}
