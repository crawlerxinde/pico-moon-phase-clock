#include "battery.h"

#include <Arduino.h>
#include <hardware/adc.h>

namespace
{
    constexpr int   kBatteryAdcPin = 40;
    constexpr int   kBatteryAdcChannel = 0;
    constexpr float kAdcFullScale = 4095.0f;
    constexpr float kAdcReference = 3.3f;
    constexpr float kDividerRatio = 3.0f; // board divider is 3:1

    unsigned long lastReadMs = 0;
    bool  initialized = false;
    bool  batteryPresent = false;
    float filteredVoltage = 0.0f;
    int   batteryPercent = 0;

    /* Single-cell Li-ion light-load discharge curve, same as Waveshare's
     * reference firmware. */
    int percentFromVoltage(float v)
    {
        static const float volt[] = {
            4.20f, 4.10f, 4.00f, 3.90f, 3.80f, 3.75f,
            3.70f, 3.65f, 3.60f, 3.50f, 3.40f, 3.30f, 3.20f
        };
        static const int pct[] = {
            100, 90, 80, 65, 50, 40, 32, 25, 20, 12, 7, 3, 0
        };
        const int n = (int)(sizeof(pct) / sizeof(pct[0]));
        if (v >= volt[0]) return 100;
        if (v <= volt[n - 1]) return 0;
        for (int i = 0; i < n - 1; i++) {
            if (v <= volt[i] && v >= volt[i + 1]) {
                float f = (v - volt[i + 1]) / (volt[i] - volt[i + 1]);
                return pct[i + 1] + (int)((pct[i] - pct[i + 1]) * f + 0.5f);
            }
        }
        return 0;
    }
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

    void update(bool force)
    {
        if (!initialized) return;

        unsigned long nowMs = millis();
        if (!force && lastReadMs != 0 && (nowMs - lastReadMs) < 1500UL) return;
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
        batteryPercent = batteryPresent ? percentFromVoltage(filteredVoltage) : 0;
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
