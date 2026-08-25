// DS3231 I2C real-time clock driver — see rtc_ds3231.h for the model.

#include "rtc_ds3231.h"

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>

namespace {
    constexpr uint8_t DS3231_ADDR    = 0x68;
    constexpr uint8_t REG_TIME       = 0x00;  // seconds..year, 7 BCD registers
    constexpr uint8_t REG_STATUS     = 0x0F;
    constexpr uint8_t STATUS_OSF     = 0x80;  // oscillator-stop flag (power lost)

    // 2024-01-01 UTC — floor for "the clock has actually been set".
    constexpr time_t  MIN_VALID_UTC  = 1704067200L;

    bool     s_present    = false;
    bool     s_valid      = false;
    uint32_t s_lastWrite  = 0;

    inline uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
    inline uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

    // Days since 1970-01-01 for a civil UTC Y/M/D (Howard Hinnant's algorithm).
    // Portable UTC->unix without relying on timegm/TZ.
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2);
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
        unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097L + (long)doe - 719468L;
    }

    bool readReg(uint8_t reg, uint8_t* buf, uint8_t len) {
        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom((int)DS3231_ADDR, (int)len) != len) return false;
        for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
        return true;
    }

    bool writeReg(uint8_t reg, const uint8_t* buf, uint8_t len) {
        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(reg);
        for (uint8_t i = 0; i < len; i++) Wire.write(buf[i]);
        return Wire.endTransmission() == 0;
    }
}

bool Ds3231::begin() {
    Wire.beginTransmission(DS3231_ADDR);
    s_present = (Wire.endTransmission() == 0);
    if (!s_present) {
        Serial.println("[RTC] DS3231 not found at 0x68");
        return false;
    }

    // Oscillator-stop flag is latched whenever the RTC loses power. If set, its
    // time is stale garbage — leave the system clock alone and wait for a GPS/
    // NTP/PigSync sync, which write-back will then persist (clearing OSF).
    uint8_t status = 0;
    if (readReg(REG_STATUS, &status, 1) && (status & STATUS_OSF)) {
        s_valid = false;
        Serial.println("[RTC] DS3231 present, oscillator was stopped — needs a time sync");
        return true;
    }

    time_t utc;
    if (readUtc(utc)) {
        struct timeval tv;
        tv.tv_sec = utc;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        s_valid = true;
        Serial.printf("[RTC] DS3231 seeded system clock: %lu (UTC)\n", (unsigned long)utc);
    } else {
        Serial.println("[RTC] DS3231 present but time read was invalid");
    }
    return true;
}

bool Ds3231::isPresent()    { return s_present; }
bool Ds3231::hasValidTime() { return s_present && s_valid; }

bool Ds3231::readUtc(time_t& out) {
    if (!s_present) return false;

    uint8_t r[7];
    if (!readReg(REG_TIME, r, 7)) return false;

    uint8_t sec = bcd2bin(r[0] & 0x7F);
    uint8_t min = bcd2bin(r[1] & 0x7F);
    uint8_t hour;
    if (r[2] & 0x40) {                       // 12-hour mode (if set externally)
        hour = bcd2bin(r[2] & 0x1F) % 12;
        if (r[2] & 0x20) hour += 12;         // PM bit
    } else {                                 // 24-hour mode (how we write it)
        hour = bcd2bin(r[2] & 0x3F);
    }
    uint8_t day   = bcd2bin(r[4] & 0x3F);
    uint8_t month = bcd2bin(r[5] & 0x1F);    // ignore century bit
    int     year  = 2000 + bcd2bin(r[6]);

    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || min > 59 || sec > 59 || year < 2024) {
        return false;                        // unset / corrupt reading
    }

    long days = daysFromCivil(year, month, day);
    out = (time_t)days * 86400L + hour * 3600L + min * 60L + sec;
    return true;
}

bool Ds3231::writeUtc(time_t utc) {
    if (!s_present) return false;

    struct tm tmv;
    gmtime_r(&utc, &tmv);

    uint8_t r[7];
    r[0] = bin2bcd((uint8_t)tmv.tm_sec);
    r[1] = bin2bcd((uint8_t)tmv.tm_min);
    r[2] = bin2bcd((uint8_t)tmv.tm_hour);              // 24-hour mode (bit6 = 0)
    r[3] = (uint8_t)(tmv.tm_wday + 1);                 // DS3231 day-of-week 1..7
    r[4] = bin2bcd((uint8_t)tmv.tm_mday);
    r[5] = bin2bcd((uint8_t)(tmv.tm_mon + 1));         // century bit7 = 0 (20xx)
    r[6] = bin2bcd((uint8_t)((tmv.tm_year + 1900) - 2000));
    if (!writeReg(REG_TIME, r, 7)) return false;

    // Time is now valid — clear the oscillator-stop flag.
    uint8_t status = 0;
    if (readReg(REG_STATUS, &status, 1)) {
        status &= ~STATUS_OSF;
        writeReg(REG_STATUS, &status, 1);
    }
    s_valid = true;
    return true;
}

void Ds3231::writeBackFromSystem() {
    if (!s_present) return;

    time_t now = time(nullptr);
    if (now < MIN_VALID_UTC) return;                   // system clock not set yet

    uint32_t ms = millis();
    if (s_lastWrite != 0 && (ms - s_lastWrite) < 600000UL) return;  // <=1/10min

    if (writeUtc(now)) {
        s_lastWrite = ms;
        Serial.printf("[RTC] DS3231 write-back: %lu (UTC)\n", (unsigned long)now);
    }
}
