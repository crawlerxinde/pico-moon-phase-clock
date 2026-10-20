#pragma once

#include <stdint.h>

namespace Astronomy
{
    void begin();
    void update(bool force = false);

    /* Screen-space angle of the terminator line, in degrees.  The value is
     * derived from the Sun's position angle at the Moon (chi), the local
     * parallactic angle (q), and the baked terminator angle of the selected
     * static asset frame. */
    float moonScreenAngleDeg();

    /* Recompute and apply the moon image rotation without recalculating the
     * rise/set events.  Used when the user edits the time in Settings. */
    void updateOrientation();

    /* Print P, q and the resulting screen angle for calibration. */
    void printOrientation();
}
