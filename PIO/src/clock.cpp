#include "clock.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <string.h>

#include "credential.h"

namespace
{
    constexpr uint8_t  RTC_ADDR       = 0x51;
    constexpr int      RTC_SDA_PIN    = 6;
    constexpr int      RTC_SCL_PIN    = 7;
    constexpr uint32_t RTC_I2C_HZ     = 400000;

    Clock::DateTime lastTime = {2026, 1, 1, 0, 0, 0, 0};
    bool     haveTime        = false;
    bool     rtc_ok          = false;
    bool     dayChanged      = true;
    unsigned long lastReadMs = 0;
    unsigned long lastTickMs = 0;

    const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *months[]   = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    uint8_t decToBcd(int v) { return (uint8_t)((v / 10 * 16) + (v % 10)); }
    int     bcdToDec(uint8_t v) { return (v / 16 * 10) + (v % 16); }

    int weekdayFromDate(int y, int m, int d)
    {
        int64_t days = Clock::daysFromCivil(y, (unsigned)m, (unsigned)d);
        /* 1970-01-01 was a Thursday (4). */
        int wd = (int)((days + 4) % 7);
        if (wd < 0) wd += 7;
        return wd;
    }

    void buildTime(Clock::DateTime &dt)
    {
        char mon[4] = {0};
        int day = 1, year = 2026, hour = 12, minute = 0, second = 0;
        sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
        sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

        dt.year   = year;
        dt.month  = 1;
        dt.day    = day;
        dt.hour   = hour;
        dt.minute = minute;
        dt.second = second;

        for (int i = 0; i < 12; i++) {
            if (strcmp(mon, months[i]) == 0) {
                dt.month = i + 1;
                break;
            }
        }
        dt.weekday = weekdayFromDate(dt.year, dt.month, dt.day);
    }

    bool readRtc(Clock::DateTime &dt)
    {
        uint8_t reg = 0x04;
        uint8_t buf[7] = {0};

        if (i2c_write_blocking(i2c1, RTC_ADDR, &reg, 1, true) < 0) return false;
        if (i2c_read_blocking(i2c1, RTC_ADDR, buf, sizeof(buf), false) < 0) return false;

        /* Bit 7 of the seconds register is the oscillator-stop flag. */
        if (buf[0] & 0x80) return false;

        int sec   = bcdToDec(buf[0] & 0x7F);
        int min   = bcdToDec(buf[1] & 0x7F);
        int hour  = bcdToDec(buf[2] & 0x3F);
        int day   = bcdToDec(buf[3] & 0x3F);
        int month = bcdToDec(buf[5] & 0x1F);
        int year  = bcdToDec(buf[6]) + 1970;

        if (sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 ||
            month < 1 || month > 12 || year < 2020 || year > 2099) {
            return false;
        }

        dt.year    = year;
        dt.month   = month;
        dt.day     = day;
        dt.hour    = hour;
        dt.minute  = min;
        dt.second  = sec;
        dt.weekday = weekdayFromDate(year, month, day);
        return true;
    }

    bool writeRtc(const Clock::DateTime &dt)
    {
        uint8_t buf[8] = {
            0x04,
            decToBcd(dt.second),
            decToBcd(dt.minute),
            decToBcd(dt.hour),
            decToBcd(dt.day),
            decToBcd(dt.weekday),
            decToBcd(dt.month),
            decToBcd(dt.year - 1970)
        };
        return i2c_write_blocking(i2c1, RTC_ADDR, buf, sizeof(buf), false) == (int)sizeof(buf);
    }

    void formatTime(const Clock::DateTime &t)
    {
        bool newDay = !haveTime || t.day != lastTime.day || t.month != lastTime.month ||
                      t.year != lastTime.year;
        if (newDay) dayChanged = true;

        lastTime = t;
        haveTime = true;

        int hour12 = t.hour % 12;
        if (hour12 == 0) hour12 = 12;
        const char *ampm = (t.hour < 12) ? "AM" : "PM";
        isAMTime = (t.hour < 12);

        snprintf(timeString12hr, sizeof(timeString12hr), "%02d:%02d", hour12, t.minute);
        snprintf(timeString24hr, sizeof(timeString24hr), "%02d:%02d", t.hour, t.minute);
        snprintf(secondsString, sizeof(secondsString), "%02d", t.second);
        snprintf(ampmString, sizeof(ampmString), "%s", ampm);
        snprintf(dateString, sizeof(dateString), "%s %02d %s %04d",
                 weekdays[t.weekday % 7], t.day, months[t.month - 1], t.year);
    }

    void applyLabels()
    {
        lv_label_set_text(ui_Screen1_Label_time, timeString12hr);
        lv_label_set_text(ui_Screen1_Label_ampm, ampmString);
        lv_label_set_text(ui_Screen1_Label_date, dateString);
        lv_label_set_text(ui_Screen1_Label_sec, secondsString);
    }
}

char timeString12hr[9] = {0};
char timeString24hr[6] = {0};
char dateString[24]    = {0};
char secondsString[3]  = "00";
char ampmString[3]     = "AM";
bool isAMTime          = true;

namespace Clock
{
    int64_t daysFromCivil(int y, unsigned m, unsigned d)
    {
        y -= m <= 2;
        const int64_t era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);
        const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (int64_t)doe - 719468;
    }

    void begin()
    {
        i2c_init(i2c1, RTC_I2C_HZ);
        gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C);
        gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(RTC_SDA_PIN);
        gpio_pull_up(RTC_SCL_PIN);

        /* CTRL1: normal mode, 24-hour, 12.5 pF internal load capacitance. */
        uint8_t ctrl[2] = {0x00, 0x01};
        i2c_write_blocking(i2c1, RTC_ADDR, ctrl, sizeof(ctrl), false);

        DateTime dt;
        if (readRtc(dt)) {
            rtc_ok = true;
            lastTime = dt;
            haveTime = true;
            formatTime(dt);
            dayChanged = true;
        } else {
            /* First boot / dead battery: seed the RTC with the firmware
             * build time.  Use the serial command "T YYYY-MM-DD HH:MM:SS"
             * (or edit this block) to correct it. */
            buildTime(dt);
            rtc_ok = writeRtc(dt);
            lastTime = dt;
            haveTime = true;
            formatTime(dt);
            dayChanged = true;
        }

        lastReadMs = lastTickMs = millis();
        applyLabels();
    }

    void update()
    {
        unsigned long nowMs = millis();

        /* Advance the software fallback every second, even if the RTC is
         * missing, so the clock still behaves like a clock. */
        if (nowMs - lastTickMs >= 1000) {
            unsigned long elapsed = (nowMs - lastTickMs) / 1000;
            lastTickMs += elapsed * 1000;
            if (!rtc_ok) {
                lastTime.second += (int)elapsed;
                while (lastTime.second >= 60) { lastTime.second -= 60; lastTime.minute++; }
                while (lastTime.minute >= 60) { lastTime.minute -= 60; lastTime.hour++; }
                if (lastTime.hour >= 24) {
                    lastTime.hour = 0;
                    /* Very small software calendar fallback. */
                    int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
                    bool leap = (lastTime.year % 4 == 0) &&
                                ((lastTime.year % 100 != 0) || (lastTime.year % 400 == 0));
                    if (leap) mdays[1] = 29;
                    lastTime.day++;
                    if (lastTime.day > mdays[lastTime.month - 1]) {
                        lastTime.day = 1;
                        lastTime.month++;
                        if (lastTime.month > 12) { lastTime.month = 1; lastTime.year++; }
                    }
                    lastTime.weekday = weekdayFromDate(lastTime.year, lastTime.month, lastTime.day);
                }
                formatTime(lastTime);
                applyLabels();
            }
        }

        if (nowMs - lastReadMs < 1000) return;
        lastReadMs = nowMs;

        DateTime dt;
        if (readRtc(dt)) {
            rtc_ok = true;
            /* Detect rollover and refresh the labels. */
            formatTime(dt);
            applyLabels();
        } else {
            rtc_ok = false;
        }
    }

    void applyToUI()
    {
        applyLabels();
    }

    DateTime now()
    {
        return lastTime;
    }

    time_t localEpoch()
    {
        int64_t days = daysFromCivil(lastTime.year, (unsigned)lastTime.month, (unsigned)lastTime.day);
        return (time_t)(days * 86400 + lastTime.hour * 3600 + lastTime.minute * 60 + lastTime.second);
    }

    time_t utcEpoch()
    {
        return localEpoch() - gmtOffset_sec - daylightOffset_sec;
    }

    bool dateChanged()
    {
        bool changed = dayChanged;
        dayChanged = false;
        return changed;
    }

    bool rtcPresent()
    {
        return rtc_ok;
    }

    bool setDateTime(int year, int month, int day, int hour, int minute, int second)
    {
        if (year < 2020 || year > 2099 || month < 1 || month > 12 ||
            day < 1 || day > 31 || hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 || second < 0 || second > 59) {
            return false;
        }

        DateTime dt;
        dt.year = year;
        dt.month = month;
        dt.day = day;
        dt.hour = hour;
        dt.minute = minute;
        dt.second = second;
        dt.weekday = weekdayFromDate(year, month, day);

        if (!writeRtc(dt)) return false;

        rtc_ok = true;
        lastTime = dt;
        haveTime = true;
        formatTime(dt);
        dayChanged = true;
        applyLabels();
        return true;
    }
}
