#include "battery.h"

#include <Arduino.h>
#include <hardware/adc.h>

namespace
{
    constexpr int   kBatteryAdcPin = 40;
    constexpr int   kBatteryAdcChannel = 0;
    constexpr float kAdcFullScale = 4095.0f;
    constexpr float kAdcReference = 3.3f;
    constexpr float kDividerRatio = 2.0f;

    unsigned long lastReadMs = 0;
    bool  initialized = false;
    bool  batteryPresent = false;
    float filteredVoltage = 0.0f;
    int   batteryPercent = 0;
}

namespace Battery
{
    void begin()
    {
        adc_init();
        adc_gpio_init(kBatteryAdcPin);
        adc_select_input(kBatteryAdcChannel);
        initialized = true;
        update();
    }

    void update()
    {
        if (!initialized) return;

        unsigned long nowMs = millis();
        if (lastReadMs != 0 && (nowMs - lastReadMs) < 1500UL) return;
        lastReadMs = nowMs;

        uint32_t sum = 0;
        for (int i = 0; i < 16; i++) {
            sum += adc_read();
        }

        float raw = (float)sum / 16.0f;
        float voltage = raw * (kAdcReference / kAdcFullScale) * kDividerRatio;

        if (filteredVoltage <= 0.0f) {
            filteredVoltage = voltage;
        } else {
            filteredVoltage = filteredVoltage * 0.75f + voltage * 0.25f;
        }

        /* With no 18650 installed the divider is pulled to ground.  Treat a
         * near-zero reading as "USB power" rather than 0 % battery. */
        batteryPresent = filteredVoltage >= 3.0f;

        if (batteryPresent) {
            float percent = (filteredVoltage - 3.3f) / (4.2f - 3.3f) * 100.0f;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 100.0f) percent = 100.0f;
            batteryPercent = (int)(percent + 0.5f);
        } else {
            batteryPercent = 0;
        }
    }

    bool present()
    {
        return batteryPresent;
    }

    float voltage()
    {
        return filteredVoltage;
    }

    int percent()
    {
        return batteryPercent;
    }
}
