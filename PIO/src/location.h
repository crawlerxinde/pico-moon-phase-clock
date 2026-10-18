#pragma once

#include <stdint.h>
#include <time.h>

namespace Location
{
    struct City
    {
        const char *name;
        float       lat;
        float       lon;
        int         stdOffsetSec;
        uint8_t     dstRule; // 0 = none, 1 = EU, 2 = US, 3 = Australia
    };

    /* Number of selectable cities. */
    int count();

    /* Load the persisted selection (defaults to Beijing). */
    void begin();

    /* Current selection. */
    int index();
    const City &current();

    /* Select a city by index.  Persists the choice to EEPROM. */
    bool setIndex(int index);

    /* City at an index, or the current city when out of range. */
    const City &at(int index);

    /* UTC offset for a local calendar date. */
    int offsetForDate(int year, int month, int day);
    int offsetForIndex(int index, int year, int month, int day);

    /* UTC offset that applies at a UTC instant (used when changing city). */
    int offsetForUtc(time_t utc);

    /* Convenience accessors for the current city. */
    const char *city();
    float latitude();
    float longitude();
}
