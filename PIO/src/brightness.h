#pragma once

#include <stdint.h>

namespace Brightness
{
    /* Store the initial brightness without touching the backlight yet. */
    void begin(uint8_t initialPercent);

    /* Apply the current stored value to the panel backlight. */
    void apply();

    /* Set 0..100; applies immediately when the screen is on. */
    void set(int percent);

    /* Adjust by delta, clamped to 35..100 for gesture/slider use. */
    void adjust(int delta);

    int  current();
    int  minimum();
    bool screenOn();
    void toggleScreen();
    void forceOff();
}
