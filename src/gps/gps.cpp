// GPS AT668 implementation

#include "gps.h"
#include "../core/config.h"
#include "../core/sdlog.h"
#include "../piglet/mood.h"
#include "../ui/display.h"
#include "../core/rtc_ds3231.h"
#include "../core/timeutil.h"
#include <sys/time.h>   // settimeofday — sync the system clock from GPS UTC

// Days since 1970-01-01 for a civil (UTC) Y/M/D (Howard Hinnant's algorithm).
// Portable UTC->unix without relying on timegm/TZ. Used to set the system clock
// from GPS so day/night (moon/stars vs sun) works without PigSync.
static long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097L + (long)doe - 719468L;
}

// Pancake (ESP32-C5) GPS is on UART1. ESP RX = GPIO14, ESP TX = GPIO13.
// ESP32Marauder MARAUDER_PANCAKE names its macros from the GPS module's side
// (GPS_TX=14, GPS_RX=13) and passes them as begin(baud, cfg, GPS_TX, GPS_RX),
// i.e. the ESP receives on 14 (wired to the module TX) and transmits on 13.
// Serial2 on the C5 is the LP-UART (pins 4/5 only) and fails on GPS pins, so
// force the correct bus+pins on Pancake. (Arduino begin() is rxPin, then txPin.)
#ifdef PORKCHOP_PANCAKE
  #define GPS_UART   Serial1
  #define GPS_FORCE_PINS(rx, tx) do { (rx) = 14; (tx) = 13; } while (0)
#else
  #define GPS_UART   Serial2
  #define GPS_FORCE_PINS(rx, tx) do { } while (0)
#endif

// Static members
TinyGPSPlus* GPS::gps = nullptr;
HardwareSerial* GPS::serial = nullptr;
bool GPS::active = false;
GPSData GPS::currentData = {0};
uint32_t GPS::fixCount = 0;
uint32_t GPS::lastFixTime = 0;
uint32_t GPS::lastUpdateTime = 0;
uint32_t GPS::detectedBaud = 0;
SemaphoreHandle_t GPS::mutex = nullptr;

// Listen at `baud` for ~1.2s; true once we see a '$' and TinyGPS parses a valid
// (checksum-passing) sentence — proof this baud is correct. Mirrors ESP32Marauder.
bool GPS::probeBaud(uint32_t baud, uint8_t rxPin, uint8_t txPin) {
    if (!gps) gps = new TinyGPSPlus();
    GPS_UART.end();
    delay(50);
    GPS_UART.begin(baud, SERIAL_8N1, rxPin, txPin);

    uint32_t start = millis();
    bool sawDollar = false;
    uint32_t baseChecksums = gps->passedChecksum();
    while (millis() - start < 1200) {
        while (GPS_UART.available()) {
            char c = GPS_UART.read();
            if (c == '$') sawDollar = true;
            gps->encode(c);
            if (sawDollar && gps->passedChecksum() > baseChecksums) return true;
        }
        delay(1);
    }
    return false;
}

// Try 115200, then 9600 (factory default) and force the module up to 115200 via
// the PCAS command, then 38400. Returns the working baud (0 if none). On return
// the UART is left open at that baud. Mirrors ESP32Marauder's probe order.
uint32_t GPS::detectBaud(uint8_t rxPin, uint8_t txPin) {
    if (probeBaud(115200, rxPin, txPin)) return 115200;

    if (probeBaud(9600, rxPin, txPin)) {
        // AT6558/ATGM336H: switch to 115200 baud, then confirm it took.
        GPS_UART.print("$PCAS01,5*19\r\n");
        GPS_UART.flush();
        delay(200);
        if (probeBaud(115200, rxPin, txPin)) return 115200;
        probeBaud(9600, rxPin, txPin);   // revert listener to the working baud
        return 9600;
    }

    if (probeBaud(38400, rxPin, txPin)) return 38400;

    probeBaud(9600, rxPin, txPin);       // leave port open at a sane default
    return 0;
}

void GPS::init(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    // GPS source now auto-configured via GPSSource enum in config
    // Pin selection happens in Config::load() based on gpsSource setting
    Serial.printf("[GPS] Init: RX=%d, TX=%d, baud=%lu\n", rxPin, txPin, baud);

    if (gps == nullptr) gps = new TinyGPSPlus();

    // Create mutex for thread safety
    if (mutex == nullptr) {
        mutex = xSemaphoreCreateMutex();
    }
    
    GPS_FORCE_PINS(rxPin, txPin);

#ifdef PORKCHOP_PANCAKE
    // Built-in module boots at 9600 but may already be at 115200 (e.g. after
    // Marauder ran). Probe for the real baud instead of trusting a fixed value.
    // (Gated to Pancake: a probe would stall boot ~4s on boards with no GPS.)
    uint32_t detected = detectBaud(rxPin, txPin);
    detectedBaud = detected;
    uint32_t useBaud = detected ? detected : baud;
    GPS_UART.end();
    delay(50);
    GPS_UART.begin(useBaud, SERIAL_8N1, rxPin, txPin);
    Serial.printf("[GPS] RX=%d TX=%d, detected baud=%lu (using %lu)\n",
                  rxPin, txPin, detected, useBaud);
#else
    GPS_UART.begin(baud, SERIAL_8N1, rxPin, txPin);
#endif
    serial = &GPS_UART;
    active = true;

    // Clear initial data - safe to use portMAX_DELAY during init (not a hot path, mutex just created)
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        memset(&currentData, 0, sizeof(GPSData));
        currentData.valid = false;
        currentData.fix = false;
        xSemaphoreGive(mutex);
    }
}

void GPS::reinit(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    // Stop existing serial connection
    if (serial) {
        GPS_UART.end();
        serial = nullptr;
        active = false;
    }
    
    // Small delay to let hardware settle
    delay(50);
    
    // Re-initialize with new parameters (re-detect baud since pins may have changed)
    GPS_FORCE_PINS(rxPin, txPin);
#ifdef PORKCHOP_PANCAKE
    uint32_t detected = detectBaud(rxPin, txPin);
    detectedBaud = detected;
    uint32_t useBaud = detected ? detected : baud;
    GPS_UART.end();
    delay(50);
    GPS_UART.begin(useBaud, SERIAL_8N1, rxPin, txPin);
#else
    GPS_UART.begin(baud, SERIAL_8N1, rxPin, txPin);
#endif
    serial = &GPS_UART;
    active = true;
    
    // Reset GPS state - safe to use portMAX_DELAY during reinit (configuration path, not hot path)
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        memset(&currentData, 0, sizeof(GPSData));
        currentData.valid = false;
        currentData.fix = false;
        xSemaphoreGive(mutex);
    }
    
    // GPS logs silenced - pig prefers stealth
    // Serial.printf("[GPS] Re-initialized on pins RX:%d TX:%d @ %d baud\n", rxPin, txPin, baud);
}

void GPS::update() {
    if (!active || serial == nullptr) return;
    
    processSerial();
    
    uint32_t now = millis();
    
    // Update data periodically
    if (now - lastUpdateTime > 100) {
        updateData();
        lastUpdateTime = now;
    }
}

void GPS::processSerial() {
    if (!serial) return;  // Safety check
    
    static uint32_t lastDebugTime = 0;
    static uint32_t bytesProcessed = 0;
    uint32_t processedThisCall = 0;
    const uint32_t maxBytesPerCall = 128; // Limit processing per call to prevent WDT
    
    while (serial->available() > 0 && processedThisCall < maxBytesPerCall) {
        char c = serial->read();
        if (c != -1) { // Valid byte read
            gps->encode(c);
            bytesProcessed++;
            processedThisCall++;
        }
    }
    
    // Yield occasionally during heavy processing to prevent WDT
    if (processedThisCall > 0 && processedThisCall % 32 == 0) {
        yield(); // Allow other tasks to run
    }
    
    // GPS debug logs silenced - pig prefers stealth
    // Uncomment for debugging:
    // uint32_t now = millis();
    // if (now - lastDebugTime >= 5000) {
    //     Serial.printf("[GPS] Bytes: %lu, Sats: %d, Valid: %s\n", bytesProcessed, gps->satellites.value(), gps->location.isValid() ? "Y" : "N");
    //     lastDebugTime = now;
    // }
}

void GPS::updateData() {
    if (mutex == nullptr) return;  // FIX: Prevent crash if GPS not initialized

    // Sync the system clock from GPS UTC so day/night (moon/stars vs sun) works
    // without needing PigSync. GPS gives UTC; isNightTime()/getTimeString() apply
    // the user's timezoneOffset. Re-sync every 10 min to correct RTC drift.
    if (gps->time.isValid() && gps->date.isValid() && gps->date.year() >= 2024) {
        static uint32_t lastClockSync = 0;
        uint32_t nowMs = millis();
        if (lastClockSync == 0 || nowMs - lastClockSync > 600000UL) {
            long days = daysFromCivil(gps->date.year(), gps->date.month(), gps->date.day());
            time_t utc = (time_t)days * 86400L + gps->time.hour() * 3600
                       + gps->time.minute() * 60 + gps->time.second();
            struct timeval tv;
            tv.tv_sec = utc;
            tv.tv_usec = 0;
            settimeofday(&tv, nullptr);
            lastClockSync = nowMs;
            Ds3231::writeBackFromSystem();  // persist fresh GPS time to the RTC
        }
    }

    // Get current GPS data safely
    bool valid = gps->location.isValid();
    double latitude = gps->location.lat();
    double longitude = gps->location.lng();
    double altitude = gps->altitude.meters();
    float speed = gps->speed.kmph();
    float course = gps->course.deg();
    uint8_t satellites = gps->satellites.value();
    uint16_t hdop = gps->hdop.value();
    uint32_t date = gps->date.isValid() ? gps->date.value() : 0;
    uint32_t time = gps->time.isValid() ? gps->time.value() : 0;
    uint32_t age = gps->location.age();
    bool fix = valid && (age < 30000);
    
    // Update shared data atomically and check for fix changes - use timeout to prevent WDT
    bool hadFix = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
        hadFix = currentData.fix;
        
        currentData.latitude = latitude;
        currentData.longitude = longitude;
        currentData.altitude = altitude;
        currentData.speed = speed;
        currentData.course = course;
        currentData.satellites = satellites;
        currentData.hdop = hdop;
        currentData.date = date;
        currentData.time = time;
        currentData.valid = valid;
        currentData.age = age;
        currentData.fix = fix;
        
        xSemaphoreGive(mutex);
    }
    
    // Process fix changes outside the mutex to avoid blocking
    if (fix && !hadFix) {
        // Increment fix count safely - use timeout to prevent WDT
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
            fixCount++;
            lastFixTime = millis();
            xSemaphoreGive(mutex);
        }
        Mood::onGPSFix();
        Display::setGPSStatus(true);
        Serial.println("[GPS] Fix acquired!");
        SDLog::log("GPS", "Fix acquired (sats: %d)", satellites);
    } else if (!fix && hadFix) {
        Mood::onGPSLost();
        Display::setGPSStatus(false);
        Serial.println("[GPS] Fix lost");
        SDLog::log("GPS", "Fix lost");
    }
}

void GPS::sleep() {
    if (!active) return;
    if (!serial) return;  // Safety check

    // AT6668 (ATGM336H) does not support u-blox UBX protocol.
    // Stop UART to cease processing and reduce CPU overhead.
    GPS_UART.end();
    serial = nullptr;
    active = false;
    Serial.println("[GPS] Entering sleep mode (UART stopped)");
}

void GPS::wake() {
    if (active) return;

    // Restart UART to resume GPS data processing.
    // AT6668 (ATGM336H) runs continuously — re-opening the port is sufficient.
    // Reuse the detected baud (module keeps its baud while the UART is closed).
    uint8_t rxPin = Config::gps().rxPin;
    uint8_t txPin = Config::gps().txPin;
    uint32_t baud = detectedBaud ? detectedBaud : Config::gps().baudRate;
    GPS_FORCE_PINS(rxPin, txPin);
    GPS_UART.begin(baud, SERIAL_8N1, rxPin, txPin);
    serial = &GPS_UART;
    active = true;
    Serial.println("[GPS] Waking up (UART restarted)");
}

void GPS::ensureContinuousMode() {
    // AT6668 (ATGM336H) runs continuously by default.
    // If UART was stopped (sleep), restart it. Otherwise just ensure flag is set.
    if (!serial) {
        uint8_t rxPin = Config::gps().rxPin;
        uint8_t txPin = Config::gps().txPin;
        uint32_t baud = detectedBaud ? detectedBaud : Config::gps().baudRate;
        GPS_FORCE_PINS(rxPin, txPin);
        GPS_UART.begin(baud, SERIAL_8N1, rxPin, txPin);
        serial = &GPS_UART;
    }
    active = true;
    Serial.println("[GPS] Continuous mode enforced");
}

void GPS::setPowerMode(bool isActive) {
    if (isActive) {
        wake();
    } else {
        sleep();
    }
}

bool GPS::isActive() {
    return active;
}

bool GPS::hasFix() {
    if (mutex == nullptr) return false;  // FIX: Prevent crash if GPS not initialized
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        bool result = currentData.fix;
        xSemaphoreGive(mutex);
        return result;
    }
    return false; // Return safe value if mutex unavailable
}

GPSData GPS::getData() {
    GPSData data = {};
    if (mutex == nullptr) return data;  // FIX: Prevent crash if GPS not initialized
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        data = currentData;
        xSemaphoreGive(mutex);
    }
    return data;
}

bool GPS::getLocationString(char* out, size_t len) {
    if (!out || len == 0) return false;
    if (mutex == nullptr) {  // FIX: Prevent crash if GPS not initialized
        strncpy(out, "No GPS", len - 1);
        out[len - 1] = '\0';
        return false;
    }
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        if (currentData.fix) {
            int written = snprintf(out, len, "%.6f,%.6f", 
                                   currentData.latitude, currentData.longitude);
            xSemaphoreGive(mutex);
            return (written > 0 && written < (int)len);
        } else {
            strncpy(out, "No fix", len - 1);
            out[len - 1] = '\0';
            xSemaphoreGive(mutex);
            return true;
        }
    } else {
        strncpy(out, "Error", len - 1);
        out[len - 1] = '\0';
        return false;
    }
}

void GPS::getTimeString(char* out, size_t len) {
    if (!out || len == 0) return;
    if (mutex == nullptr) {  // FIX: Prevent crash if GPS not initialized
        snprintf(out, len, "--:--");
        return;
    }
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        if (gps->time.isValid() && gps->date.isValid() && gps->date.year() >= 2024) {
            // GPS gives UTC. Build a UTC epoch, then let localtime_r() apply the
            // user's (DST-aware) zone — the date is required so DST resolves.
            time_t utc = TimeUtil::utcEpoch(gps->date.year(), gps->date.month(),
                                            gps->date.day(), gps->time.hour(),
                                            gps->time.minute(), gps->time.second());
            struct tm local;
            localtime_r(&utc, &local);
            Display::formatClock(out, len, local.tm_hour, local.tm_min);
        } else {
            snprintf(out, len, "--:--");
        }
        xSemaphoreGive(mutex);
    } else {
        snprintf(out, len, "ERR");
    }
}

uint32_t GPS::getFixCount() {
    if (mutex == nullptr) return 0;  // FIX: Prevent crash if GPS not initialized
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        uint32_t count = fixCount;
        xSemaphoreGive(mutex);
        return count;
    }
    return 0; // Return safe value if mutex unavailable
}

uint32_t GPS::getLastFixTime() {
    if (mutex == nullptr) return 0;  // FIX: Prevent crash if GPS not initialized
    if (xSemaphoreTake(mutex, 10 / portTICK_PERIOD_MS)) {
        uint32_t time = lastFixTime;
        xSemaphoreGive(mutex);
        return time;
    }
    return 0; // Return safe value if mutex unavailable
}
