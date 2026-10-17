#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace Display
{
    /* Landscape orientation of the 172x640 panel. */
    constexpr uint16_t Width  = 640;
    constexpr uint16_t Height = 172;

    /* Touch calibration.  The AXS15231B reports raw coordinates in a
     * 640x172 landscape frame.  These are runtime flags so they can also be
     * changed over the serial console (X / Y / S commands). */
    extern bool TouchSwapXY;
    extern bool TouchInvertX;
    extern bool TouchInvertY;

    void begin();
}
