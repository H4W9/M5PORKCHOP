#pragma once

#include <stdint.h>
#include <time.h>

// ============================================================================
// DS3231 I2C real-time clock (address 0x68)
// ----------------------------------------------------------------------------
// Optional battery-backed hardware clock. On the Pancake / Marauder V8 it hangs
// off the same I2C bus the touch panel / battery gauge already use
// (PANCAKE_I2C_SDA/SCL), so Wire is already begun by Display::init() before this
// runs. The DS3231 answers at 0x68, clear of the touch (0x38) and gauge (~0x36).
//
// The clock is stored as UTC (same convention as the GPS/NTP/PigSync sync
// paths); the display layer applies the user's timezone offset. Model:
//   * boot   -> seed the system clock from the RTC (so day/night + timestamps
//               are right immediately, before any GPS lock or WiFi sync)
//   * on any authoritative sync (GPS/NTP/PigSync) -> write the fresh time back
//               so the RTC self-corrects its ~2ppm drift and persists it.
//
// Every entry point is a safe no-op when no DS3231 is present, so the write-back
// calls can be sprinkled at the sync sites without platform #ifdefs.
// ============================================================================
namespace Ds3231 {
    // Probe the bus (call after Wire.begin()). If a DS3231 responds and holds a
    // valid, non-power-lost time, seed the system clock from it. Returns true if
    // a DS3231 answered at 0x68.
    bool begin();

    // True if a DS3231 responded at begin().
    bool isPresent();

    // True once the RTC is confirmed to hold a valid time (oscillator was not
    // stopped). False if it lost power and still needs an authoritative sync.
    bool hasValidTime();

    // Read the RTC into a UTC time_t. False if absent or the reading is invalid.
    bool readUtc(time_t& out);

    // Write a UTC time_t into the RTC and clear the oscillator-stop flag.
    bool writeUtc(time_t utc);

    // Push the current system clock (UTC) into the RTC, rate-limited to once per
    // 10 min. Call right after settimeofday() on the GPS/NTP/PigSync paths.
    // No-op if the RTC is absent or the system clock is not set yet.
    void writeBackFromSystem();
}
