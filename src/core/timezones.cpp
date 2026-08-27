// Timezone table + TZ application. See timezones.h.

#include "timezones.h"

#include <cstdlib>   // setenv
#include <ctime>     // tzset

namespace Timezones {

// Kept identical (names, POSIX strings, order) to the ASCII-Aquarium Pancake
// firmware so both agree on this shared hardware. POSIX sign is inverted vs the
// label: e.g. "Central" = CST6CDT (6h behind UTC), "China" = CST-8 (8h ahead).
const Option kOptions[] = {
    {"UTC",         "UTC0"},
    {"Hawaii",      "HST10"},
    {"Alaska",      "AKST9AKDT,M3.2.0/2,M11.1.0/2"},
    {"Pacific",     "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"Mountain",    "MST7MDT,M3.2.0/2,M11.1.0/2"},
    {"Central",     "CST6CDT,M3.2.0/2,M11.1.0/2"},
    {"Eastern",     "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"Atlantic",    "AST4ADT,M3.2.0/2,M11.1.0/2"},
    {"Newfound",    "NST3:30NDT,M3.2.0/2,M11.1.0/2"},
    {"UTC-3",       "UTC3"},
    {"UTC-2",       "UTC2"},
    {"UTC-1",       "UTC1"},
    {"UK",          "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"Central EU",  "CET-1CEST,M3.5.0/2,M10.5.0/3"},
    {"Eastern EU",  "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"UTC+3",       "UTC-3"},
    {"Iran",        "IRST-3:30"},
    {"Gulf",        "GST-4"},
    {"UTC+5",       "UTC-5"},
    {"India",       "IST-5:30"},
    {"UTC+6",       "UTC-6"},
    {"UTC+7",       "UTC-7"},
    {"China",       "CST-8"},
    {"Japan",       "JST-9"},
    {"Darwin",      "ACST-9:30"},
    {"Sydney",      "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
    {"UTC+11",      "UTC-11"},
    {"New Zealand", "NZST-12NZDT,M9.5.0/2,M4.1.0/3"},
    {"UTC+13",      "UTC-13"},
    {"UTC+14",      "UTC-14"},
};

static_assert(sizeof(kOptions) / sizeof(kOptions[0]) == (size_t)kCount,
              "Timezones::kCount must match the number of kOptions entries");

static uint8_t clampIndex(uint8_t index) {
    return (index < (uint8_t)kCount) ? index : kDefaultIndex;
}

const char* posixFor(uint8_t index) { return kOptions[clampIndex(index)].posix; }
const char* nameFor(uint8_t index)  { return kOptions[clampIndex(index)].name; }

void apply(uint8_t index) {
    setenv("TZ", posixFor(index), 1);
    tzset();
}

}  // namespace Timezones
