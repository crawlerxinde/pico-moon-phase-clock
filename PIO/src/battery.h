#pragma once

#include <stdint.h>

namespace Battery
{
    /* Battery voltage is divided by 3 and fed to GPIO40 / ADC0. */
    void begin();
    void update();

    bool  present();
    float voltage();
    int   percent();
}
