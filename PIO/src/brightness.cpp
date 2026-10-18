#include "brightness.h"

#include "lcd_3in49.h"

namespace
{
    int  currentPercent = 100;
    bool screenOnFlag = true;
}

namespace Brightness
{
    void begin(uint8_t initialPercent)
    {
        currentPercent = initialPercent;
        screenOnFlag = true;
    }

    void apply()
    {
        LCD_SetBacklight(screenOnFlag ? (uint8_t)currentPercent : 0);
    }

    void set(int percent)
    {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        currentPercent = percent;
        if (screenOnFlag) {
            LCD_SetBacklight((uint8_t)currentPercent);
        }
    }

    void adjust(int delta)
    {
        int next = currentPercent + delta;
        if (next < 10) next = 10;
        if (next > 100) next = 100;
        set(next);
    }

    int current()
    {
        return currentPercent;
    }

    bool screenOn()
    {
        return screenOnFlag;
    }

    void toggleScreen()
    {
        screenOnFlag = !screenOnFlag;
        apply();
    }

    void forceOff()
    {
        LCD_SetBacklight(0);
    }
}
