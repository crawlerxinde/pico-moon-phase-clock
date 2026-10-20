#pragma once

#include <stdint.h>

namespace Astronomy
{
    void begin();
    void update(bool force = false);

    /* Rotate the moon tile so lunar north points in the direction it would
     * have in the local sky.  The value is the position angle of the lunar
     * north pole relative to the local zenith (P - q), in degrees. */
    float moonScreenAngleDeg();

    /* Recompute and apply the moon image rotation without recalculating the
     * rise/set events.  Used when the user edits the time in Settings. */
    void updateOrientation();

    /* Print P, q and the resulting screen angle for calibration. */
    void printOrientation();
}
