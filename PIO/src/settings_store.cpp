#include "settings_store.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "audio_tick.h"
#include "brightness.h"
#include "astronomy.h"
#include "clock.h"
#include "location.h"

namespace
{
    constexpr int      EEPROM_OFFSET = 64;
    constexpr uint32_t SNAPSHOT_MAGIC = 0x534E4150UL; // "SNAP"
    constexpr uint8_t  SNAPSHOT_VERSION = 1;
    constexpr unsigned long SAVE_DEBOUNCE_MS = 1500;

    struct Snapshot
    {
        uint32_t magic;
        uint8_t  version;
        uint8_t  location;
        uint8_t  brightness;
        uint8_t  tick;
        int16_t  year;
        uint8_t  month;
        uint8_t  day;
        uint8_t  hour;
        uint8_t  minute;
        uint8_t  second;
        uint8_t  reserved;
    };

    Snapshot lastSaved;
    bool     dirty = false;
    unsigned long dirtySince = 0;

    bool snapshotValid(const Snapshot &s)
    {
        return s.magic == SNAPSHOT_MAGIC &&
               s.version == SNAPSHOT_VERSION &&
               s.location < Location::count() &&
               s.year >= 2020 && s.year <= 2069 &&
               s.month >= 1 && s.month <= 12 &&
               s.day >= 1 && s.day <= 31 &&
               s.hour <= 23 && s.minute <= 59 && s.second <= 59;
    }

    void rememberCurrent()
    {
        Clock::DateTime now = Clock::now();
        lastSaved = {
            SNAPSHOT_MAGIC,
            SNAPSHOT_VERSION,
            (uint8_t)Location::index(),
            (uint8_t)Brightness::current(),
            (uint8_t)(Audio::enabled() ? 1 : 0),
            (int16_t)now.year,
            (uint8_t)now.month,
            (uint8_t)now.day,
            (uint8_t)now.hour,
            (uint8_t)now.minute,
            (uint8_t)now.second,
            0
        };
    }

    void writeSnapshot()
    {
        rememberCurrent();
        EEPROM.put(EEPROM_OFFSET, lastSaved);
        EEPROM.commit();
        dirty = false;
    }

    bool configChanged()
    {
        return lastSaved.location != (uint8_t)Location::index() ||
               lastSaved.brightness != (uint8_t)Brightness::current() ||
               lastSaved.tick != (uint8_t)(Audio::enabled() ? 1 : 0);
    }
}

namespace SettingsStore
{
    void begin()
    {
        EEPROM.begin(sizeof(Snapshot));
        Snapshot snap;
        EEPROM.get(EEPROM_OFFSET, snap);

        if (snapshotValid(snap)) {
            if (snap.location != (uint8_t)Location::index()) {
                Location::setIndex(snap.location);
            }
            if (snap.brightness != (uint8_t)Brightness::current()) {
                Brightness::set(snap.brightness);
            }
            if (snap.tick != (uint8_t)(Audio::enabled() ? 1 : 0)) {
                Audio::setEnabled(snap.tick != 0);
            }
            if (Clock::timeWasLost()) {
                Clock::DateTime dt = {
                    (int)snap.year, (int)snap.month, (int)snap.day,
                    (int)snap.hour, (int)snap.minute, (int)snap.second, 0
                };
                if (Clock::setDateTime(dt.year, dt.month, dt.day,
                                       dt.hour, dt.minute, dt.second)) {
                    Astronomy::update(true);
                }
            }
            rememberCurrent();
        } else {
            writeSnapshot();
        }
    }

    void update()
    {
        Clock::DateTime now = Clock::now();

        if (configChanged()) {
            if (!dirty) {
                dirty = true;
                dirtySince = millis();
            }
        } else {
            dirty = false;
        }

        if (dirty && (millis() - dirtySince) >= SAVE_DEBOUNCE_MS) {
            writeSnapshot();
        }

        /* Once an hour, persist the current time as a fallback for the RTC. */
        if (now.minute == 0 &&
            (now.year != lastSaved.year || now.month != lastSaved.month ||
             now.day != lastSaved.day || now.hour != lastSaved.hour)) {
            writeSnapshot();
        }
    }

    void save()
    {
        writeSnapshot();
    }
}
