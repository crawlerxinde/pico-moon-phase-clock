#include "brightness.h"

#include "lcd_3in49.h"

namespace
{
    constexpr int MIN_PERCENT = 35;
    int  currentPercent = 100;
    bool screenOnFlag = true;
}

namespace Brightness
{
    void begin(uint8_t initialPercent)
    {
        currentPercent = initialPercent;
        if (currentPercent < MIN_PERCENT) currentPercent = MIN_PERCENT;
        if (currentPercent > 100) currentPercent = 100;
        screenOnFlag = true;
    }

    void apply()
    {
        LCD_SetBacklight(screenOnFlag ? (uint8_t)currentPercent : 0);
    }

    void set(int percent)
    {
        if (percent < MIN_PERCENT) percent = MIN_PERCENT;
        if (percent > 100) percent = 100;
        currentPercent = percent;
        if (screenOnFlag) {
            LCD_SetBacklight((uint8_t)currentPercent);
        }
    }

    void adjust(int delta)
    {
        int next = currentPercent + delta;
        if (next < MIN_PERCENT) next = MIN_PERCENT;
        if (next > 100) next = 100;
        set(next);
    }

    int current()
    {
        return currentPercent;
    }

    int minimum()
    {
        return MIN_PERCENT;
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
