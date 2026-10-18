#include "location.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "clock.h"
#include "credential.h"

namespace
{
    constexpr uint32_t kSettingsMagic   = 0x4D504331UL; // "MPC1"
    constexpr uint8_t  kSettingsVersion = 1;

    struct Persisted
    {
        uint32_t magic;
        uint8_t  version;
        uint8_t  index;
        uint16_t reserved;
    };

    struct CityRecord
    {
        const char *name;
        float       lat;
        float       lon;
        int         stdOffsetSec;
        int         dstRule;
    };

    /* The first entry follows the defaults in credential.h so the original
     * Beijing configuration remains the power-on default. */
    const CityRecord kCities[] = {
        {myCity, myLat, myLon, (int)gmtOffset_sec, 0}, // Beijing / UTC+8
        {"Paris",    48.8566f,   2.3522f,   1 * 3600,  1},
        {"London",   51.5074f,  -0.1278f,   0,         1},
        {"New York", 40.7128f, -74.0060f,  -5 * 3600,  2},
        {"Tokyo",    35.6762f, 139.6503f,   9 * 3600,  0},
        {"Sydney",  -33.8688f, 151.2093f,  10 * 3600,  3},
    };

    constexpr int kCityCount = (int)(sizeof(kCities) / sizeof(kCities[0]));

    int currentIndex = 0;

    int weekdayOf(int year, int month, int day)
    {
        int64_t days = Clock::daysFromCivil(year, (unsigned)month, (unsigned)day);
        int wd = (int)((days + 4) % 7); // 1970-01-01 was Thursday
        if (wd < 0) wd += 7;
        return wd;
    }

    int lastSunday(int year, int month)
    {
        int day = Clock::daysInMonth(year, month);
        while (weekdayOf(year, month, day) != 0) day--;
        return day;
    }

    int firstSunday(int year, int month)
    {
        int day = 1;
        while (weekdayOf(year, month, day) != 0) day++;
        return day;
    }

    int secondSunday(int year, int month)
    {
        return firstSunday(year, month) + 7;
    }

    bool dstActive(int rule, int year, int month, int day)
    {
        switch (rule) {
        case 1: // European Union: last Sunday March -> last Sunday October
            if (month > 3 && month < 10) return true;
            if (month < 3 || month > 10) return false;
            if (month == 3) return day >= lastSunday(year, 3);
            return day < lastSunday(year, 10);

        case 2: // United States: second Sunday March -> first Sunday November
            if (month > 3 && month < 11) return true;
            if (month < 3 || month > 11) return false;
            if (month == 3) return day >= secondSunday(year, 3);
            return day < firstSunday(year, 11);

        case 3: // Australia (southern): first Sunday October -> first Sunday April
            if (month > 10 || month < 4) return true;
            if (month > 4 && month < 10) return false;
            if (month == 10) return day >= firstSunday(year, 10);
            return day < firstSunday(year, 4);

        default:
            return false;
        }
    }

    void saveIndex()
    {
        Persisted settings = {
            kSettingsMagic,
            kSettingsVersion,
            (uint8_t)currentIndex,
            0
        };
        EEPROM.put(0, settings);
        EEPROM.commit();
    }
}

namespace Location
{
    int count()
    {
        return kCityCount;
    }

    void begin()
    {
        EEPROM.begin(sizeof(Persisted));

        Persisted settings;
        EEPROM.get(0, settings);
        if (settings.magic == kSettingsMagic &&
            settings.version == kSettingsVersion &&
            settings.index < kCityCount) {
            currentIndex = settings.index;
        } else {
            currentIndex = 0;
            saveIndex();
        }
    }

    int index()
    {
        return currentIndex;
    }

    const City &current()
    {
        return at(currentIndex);
    }

    const City &at(int index)
    {
        if (index < 0 || index >= kCityCount) index = currentIndex;
        static City city;
        city.name         = kCities[index].name;
        city.lat          = kCities[index].lat;
        city.lon          = kCities[index].lon;
        city.stdOffsetSec = kCities[index].stdOffsetSec;
        city.dstRule      = (uint8_t)kCities[index].dstRule;
        return city;
    }

    bool setIndex(int index)
    {
        if (index < 0 || index >= kCityCount) return false;
        if (index != currentIndex) {
            currentIndex = index;
            saveIndex();
        }
        return true;
    }

    int offsetForIndex(int index, int year, int month, int day)
    {
        if (index < 0 || index >= kCityCount) index = currentIndex;
        const CityRecord &c = kCities[index];
        int offset = c.stdOffsetSec;
        if (dstActive(c.dstRule, year, month, day)) offset += 3600;
        return offset;
    }

    int offsetForDate(int year, int month, int day)
    {
        return offsetForIndex(currentIndex, year, month, day);
    }

    int offsetForUtc(time_t utc)
    {
        const CityRecord &c = kCities[currentIndex];
        time_t approxLocal = utc + c.stdOffsetSec;
        struct tm tmValue;
        if (!gmtime_r(&approxLocal, &tmValue)) return c.stdOffsetSec;

        int offset = offsetForIndex(currentIndex,
                                    tmValue.tm_year + 1900,
                                    tmValue.tm_mon + 1,
                                    tmValue.tm_mday);
        return offset;
    }

    const char *city()
    {
        return kCities[currentIndex].name;
    }

    float latitude()
    {
        return kCities[currentIndex].lat;
    }

    float longitude()
    {
        return kCities[currentIndex].lon;
    }
}
