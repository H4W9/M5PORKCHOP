#pragma once

#include <cstdint>
#include <esp_heap_caps.h>

namespace HeapPolicy {
 // TLS gating thresholds
 // Pancake (ESP32-C5): mbedTLS record buffers + WiFi/LWIP live in PSRAM, so the
 // 35KB internal-free bar (Cardputer-era) blocked every HTTPS upload
 // (WiGLE / WPA-SEC) on the C5's ~15-25KB internal heap. 18KB is enough to start
 // the TLS session with buffers in PSRAM. Cardputer keeps the conservative value.
 // kMinContigForTls is checked against the largest block incl. PSRAM (8MB), so it
 // passes as-is on both.
 #ifdef PORKCHOP_PANCAKE
    static constexpr size_t kMinHeapForTls = 18000;
 #else
    static constexpr size_t kMinHeapForTls = 35000;
 #endif
 static constexpr size_t kMinContigForTls = 35000;
 static constexpr size_t kProactiveTlsConditioning = 45000;

 // General allocation safety thresholds
 static constexpr size_t kMinHeapForOinkNetworkAdd = 30000;

 // ============================================================
 // HANDSHAKE ALLOCATION GATE — PATCHED for Pancake (ESP32-C5)
 // ============================================================
 // The Pancake has 8MB PSRAM but limited internal SRAM (~25KB free
 // during active OINK mode). The original 60000 threshold blocked
 // ALL handshake creation because ESP.getFreeHeap() only counts
 // internal SRAM, not PSRAM. The Pancake build uses PSRAM for
 // handshake allocations, so we lower the threshold.
 //
 // Cardputer (ESP32-S3) keeps the original conservative threshold.
 // ============================================================
 #ifdef PORKCHOP_PANCAKE
    static constexpr size_t kMinHeapForHandshakeAdd = 10000;  // Pancake: 10KB (was 60000)
 #else
    static constexpr size_t kMinHeapForHandshakeAdd = 60000;  // Cardputer: original
 #endif

 static constexpr size_t kMinHeapForReconGrowth = 20000;
 static constexpr size_t kMinHeapForSpectrumGrowth = 20000;

 // Heap stabilization / recovery thresholds
 static constexpr size_t kHeapStableThreshold = 50000;
 // File-server (HTTP WebServer) start gate — internal free heap.
 // Pancake (ESP32-C5) routes WiFi/LWIP buffers to PSRAM
 // (CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP) and only ~15-25KB internal is free
 // after the driver loads, so the 40KB Cardputer figure blocked the server from
 // ever starting. The UI already operates at 12KB free (kFileServerUiMinFree),
 // so 15KB to start is safe. Cardputer keeps the conservative value.
 #ifdef PORKCHOP_PANCAKE
    static constexpr size_t kFileServerMinHeap = 15000;
 #else
    static constexpr size_t kFileServerMinHeap = 40000;
 #endif
 static constexpr size_t kFileServerMinLargest = 30000;
 static constexpr size_t kFileServerLogThreshold = 60000;
 // Per-request gate for serving the UI assets (CSS/JS). These are PROGMEM +
 // chunked, so a send needs almost no heap; the C5's internal free dips under
 // 12KB mid-serve and was returning 503 (broken/unstyled page). Lower it on the
 // Pancake so serving stays stable. (Largest is checked incl. PSRAM, so leave.)
 #ifdef PORKCHOP_PANCAKE
    static constexpr size_t kFileServerUiMinFree = 5000;
 #else
    static constexpr size_t kFileServerUiMinFree = 12000;
 #endif
 static constexpr size_t kFileServerUiMinLargest = 8000;

 // Allocation slack (allocator overhead / fragmentation cushion)
 static constexpr size_t kReserveSlackSmall = 256;
 static constexpr size_t kReserveSlackLarge = 1024;
 static constexpr size_t kPmkidAllocSlack = 256;
 static constexpr size_t kHandshakeAllocSlack = 1024;

 // Mode-specific thresholds
 static constexpr size_t kDnhInjectMinHeap = 80000;
 static constexpr size_t kPigSyncMinContig = 26000;

 // Heap health sampling/tuning
 static constexpr uint32_t kHealthSampleIntervalMs = 1000;
 static constexpr uint32_t kHealthToastDurationMs = 5000;
 static constexpr uint8_t kHealthToastMinDelta = 5;
 static constexpr uint32_t kHealthToastSettleMs = 3000;
 static constexpr uint8_t kHealthConditionTriggerPct = 65;
 static constexpr uint8_t kHealthConditionClearPct = 75;
 static constexpr float kHealthFragPenaltyScale = 0.60f;

 // Display EMA smoothing (asymmetric to absorb transient spikes)
 static constexpr float kDisplayEmaAlphaDown = 0.10f;
 static constexpr float kDisplayEmaAlphaUp = 0.20f;

 // Adaptive conditioning cooldown (replaces fixed 30s)
 static constexpr uint32_t kConditionCooldownMinMs = 15000;
 static constexpr uint32_t kConditionCooldownMaxMs = 60000;
 static constexpr uint32_t kConditionCooldownBaseMs = 30000;

 // ============================================================
 // MEMORY PRESSURE LEVELS — PATCHED for Pancake (ESP32-C5)
 // ============================================================
 // The Pancake has PSRAM (8MB) available for allocations, so
 // internal SRAM pressure alone should not trigger aggressive
 // shedding that blocks handshake capture. We lower the pressure
 // thresholds so that 25KB free internal SRAM (typical during
 // active OINK) stays at Normal/Caution instead of Warning.
 //
 // Cardputer keeps original thresholds (no PSRAM dependency).
 // ============================================================
 #ifdef PORKCHOP_PANCAKE
    static constexpr size_t kPressureLevel1Free = 25000;  // Caution (was 80000)
    static constexpr size_t kPressureLevel2Free = 15000;  // Warning (was 50000)
    static constexpr size_t kPressureLevel3Free = 8000;   // Critical (was 30000)
 #else
    static constexpr size_t kPressureLevel1Free = 80000;  // Caution
    static constexpr size_t kPressureLevel2Free = 50000;  // Warning
    static constexpr size_t kPressureLevel3Free = 30000;  // Critical
 #endif

 static constexpr float kPressureLevel1Frag = 0.60f;
 static constexpr float kPressureLevel2Frag = 0.40f;
 static constexpr float kPressureLevel3Frag = 0.25f;
 static constexpr uint32_t kPressureHysteresisMs = 3000;

 // Pressure level gates for expensive operations
 static constexpr uint8_t kMaxPressureLevelForAutoBrew = 2;
 static constexpr uint8_t kMaxPressureLevelForSDWrite = 1;

 // Watermark persistence interval (auto-save to SD)
 static constexpr uint32_t kWatermarkSaveIntervalMs = 60000;

 // Knuth's Rule monitoring (free_blocks / allocated_blocks ratio)
 static constexpr float kKnuthRatioWarning = 0.70f;

 // Growth gating (fragmentation-aware)
 static constexpr float kMinFragRatioForGrowth = 0.40f;

 // Stress test guardrail
 static constexpr size_t kStressMinHeap = 70000;

 // Runtime conditioning dwell times (used by OINK Bounce / brewHeap)
 static constexpr uint32_t kConditioningDwellMs = 3000;
 static constexpr uint32_t kConditioningStepMs = 100;
 static constexpr uint32_t kConditioningWarmupMs = 1000;
 static constexpr uint32_t kConditioningLogIntervalMs = 1000;
 static constexpr uint32_t kConditioningFinalDelayMs = 50;
 static constexpr uint32_t kBrewDefaultDwellMs = 1000;
 static constexpr uint32_t kBrewAutoDwellMs = 1200;

 // FileServer LWIP async cleanup polling
 static constexpr uint32_t kFileServerLwipWaitMaxMs = 500;
 static constexpr uint32_t kFileServerLwipPollMs = 50;

 // WiFi/BLE settle delays used during conditioning/reset
 static constexpr uint32_t kWiFiModeDelayMs = 50;
 static constexpr uint32_t kWiFiDisconnectDelayMs = 50;
 static constexpr uint32_t kWiFiShutdownDelayMs = 80;
 static constexpr uint32_t kBleStopDelayMs = 50;
 static constexpr uint32_t kBleDeinitDelayMs = 100;

 // NTP sync policy
 static constexpr int kNtpRssiMinDbm = -60;
 static constexpr uint32_t kNtpTimeoutMs = 6000;
 static constexpr uint32_t kNtpMinFreeHeap = 20000;
 static constexpr uint32_t kNtpMinContig = 8000;
 static constexpr uint32_t kNtpRetryCooldownMs = 60000;
}