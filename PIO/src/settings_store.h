#pragma once

namespace SettingsStore
{
    /* Restore the last saved location, brightness, tick setting and, when
     * the RTC has lost time, the last known clock value. */
    void begin();

    /* Periodically persist changed settings and an hourly time backup. */
    void update();

    /* Snapshot the current settings immediately (e.g. when leaving Settings). */
    void save();
}
