#pragma once

#include <ctime>

// Small header-only helpers for turning civil UTC date/time fields into a unix
// epoch, without depending on timegm/TZ. Used where a UTC time_t must be built
// from GPS or RTC fields before handing it to localtime_r() for display.
namespace TimeUtil {

// Days since 1970-01-01 for a civil (UTC) Y/M/D — Howard Hinnant's algorithm.
inline long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097L + (long)doe - 719468L;
}

// Build a UTC time_t from civil UTC fields.
inline time_t utcEpoch(int y, unsigned mo, unsigned d,
                       unsigned h, unsigned mi, unsigned s) {
    return (time_t)daysFromCivil(y, mo, d) * 86400L
         + (long)h * 3600L + (long)mi * 60L + (long)s;
}

}  // namespace TimeUtil
