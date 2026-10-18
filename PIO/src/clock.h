#pragma once

#include <stdint.h>
#include <time.h>

namespace Clock
{
    struct DateTime
    {
        int year;   // full year, e.g. 2026
        int month;  // 1..12
        int day;    // 1..31
        int hour;   // 0..23
        int minute; // 0..59
        int second; // 0..59
        int weekday; // 0 = Sunday
    };

    void begin();
    void update();
    void applyToUI();

    DateTime now();
    time_t   localEpoch();
    time_t   utcEpoch();
    bool     dateChanged();
    bool     rtcPresent();

    /* Set the PCF85063A from local date/time.  Returns false on I2C error
     * or invalid date/time.  The RTC only supports 2020..2069 here. */
    bool setDateTime(int year, int month, int day, int hour, int minute, int second);

    /* Convert a UTC instant into the RTC's local-time representation. */
    bool setFromUtc(time_t utc, int offsetSec);

    /* Helpers shared with the astronomy/location modules. */
    int64_t daysFromCivil(int year, unsigned month, unsigned day);
    int     daysInMonth(int year, int month);
}

extern char timeString12hr[9];
extern char timeString24hr[6];
extern char dateString[24];
extern char secondsString[3];
extern char ampmString[3];
extern bool isAMTime;
