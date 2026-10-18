#pragma once

namespace Audio
{
    void begin();
    void update();
    void tick(bool high);
    void setEnabled(bool enabled);
    bool enabled();

    /* Diagnostic: read the ES8311 chip ID (0x8311 expected). */
    unsigned int codecId();
}
