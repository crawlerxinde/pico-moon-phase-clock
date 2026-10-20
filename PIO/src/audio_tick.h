#pragma once

namespace Audio
{
    void begin();
    void update();
    void tick(bool high);
    /* persist=false is used for temporary previews in Settings. */
    void setEnabled(bool enabled, bool persist = true);
    bool enabled();

    /* Diagnostic: read the ES8311 chip ID (0x8311 expected). */
    unsigned int codecId();
}
