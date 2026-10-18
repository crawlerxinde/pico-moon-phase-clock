#include "location.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "clock.h"
#include "credential.h"
#include "location_data.h"

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

    int lastWeekday(int year, int month, int weekday)
    {
        int day = Clock::daysInMonth(year, month);
        while (weekdayOf(year, month, day) != weekday) day--;
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

        case 4: // New Zealand: last Sunday September -> first Sunday April
            if (month > 9 || month < 4) return true;
            if (month > 4 && month < 9) return false;
            if (month == 9) return day >= lastSunday(year, 9);
            return day < firstSunday(year, 4);

        case 5: // Chile: first Sunday September -> first Sunday April
            if (month > 9 || month < 4) return true;
            if (month > 4 && month < 9) return false;
            if (month == 9) return day >= firstSunday(year, 9);
            return day < firstSunday(year, 4);

        case 6: // Egypt: last Friday April -> last Thursday October
            if (month > 4 && month < 10) return true;
            if (month < 4 || month > 10) return false;
            if (month == 4) return day >= lastWeekday(year, 4, 5);
            return day <= lastWeekday(year, 10, 4);

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
        return kLocationCityCount;
    }

    void begin()
    {
        EEPROM.begin(sizeof(Persisted));

        Persisted settings;
        EEPROM.get(0, settings);
        if (settings.magic == kSettingsMagic &&
            settings.version == kSettingsVersion &&
            settings.index < kLocationCityCount) {
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
        if (index < 0 || index >= kLocationCityCount) index = currentIndex;
        static City city;
        if (index == 0) {
            /* Keep credential.h as the user-editable default location. */
            city.name         = myCity;
            city.lat          = myLat;
            city.lon          = myLon;
            city.stdOffsetSec = (int)gmtOffset_sec;
            city.dstRule      = 0;
        } else {
            city.name         = kLocationCities[index].name;
            city.lat          = kLocationCities[index].lat;
            city.lon          = kLocationCities[index].lon;
            city.stdOffsetSec = kLocationCities[index].stdOffsetSec;
            city.dstRule      = kLocationCities[index].dstRule;
        }
        return city;
    }

    bool setIndex(int index)
    {
        if (index < 0 || index >= kLocationCityCount) return false;
        if (index != currentIndex) {
            currentIndex = index;
            saveIndex();
        }
        return true;
    }

    int offsetForIndex(int index, int year, int month, int day)
    {
        if (index < 0 || index >= kLocationCityCount) index = currentIndex;
        if (index == 0) return (int)gmtOffset_sec;
        const LocationCityData &c = kLocationCities[index];
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
        const City &c = at(currentIndex);
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
        return at(currentIndex).name;
    }

    float latitude()
    {
        return at(currentIndex).lat;
    }

    float longitude()
    {
        return at(currentIndex).lon;
    }
}
