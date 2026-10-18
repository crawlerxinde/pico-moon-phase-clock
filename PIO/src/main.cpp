#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <hardware/clocks.h>

#include <stdlib.h>

#include "astronomy.h"
#include "clock.h"
#include "credential.h"
#include "display.h"
#include "lcd_3in49.h"

extern const lv_img_dsc_t *ui_imgset_moon_[30];

namespace
{
    constexpr int POWER_LATCH_PIN = 39;
    constexpr int POWER_BUTTON_PIN = 38;

    unsigned long bootStartMs = 0;
    bool          bootHidden = false;

    unsigned long buttonDownMs = 0;
    bool          buttonWasDown = false;
    bool          displayFlipped = false;

    char serialBuf[64];
    size_t serialLen = 0;

    void hideBootScreen()
    {
        lv_anim_del(ui_Screen1_Image_bootscreeni, NULL);
        lv_obj_add_flag(ui_Screen1_Image_bootscreeni,
                        LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_add_flag(ui_Screen1_Label_bootscreen,
                        LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(ui_Screen1_Container_Container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Screen1_Image_Moon, LV_OBJ_FLAG_HIDDEN);
    }

    void showPlaceholders()
    {
        lv_label_set_text(ui_Screen1_Label_sunrise,  "--");
        lv_label_set_text(ui_Screen1_Label_sunset,   "--");
        lv_label_set_text(ui_Screen1_Label_moonrise, "--");
        lv_label_set_text(ui_Screen1_Label_moonset,  "--");
        lv_label_set_text(ui_Screen1_Label_Phase,    myCity);
        lv_obj_clear_flag(ui_Screen1_Image_sun_moon,
                          LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(ui_Screen1_Panel_border, LV_OBJ_FLAG_HIDDEN);
    }

    void captureScreenshot()
    {
        lv_obj_t *screen = lv_scr_act();
        if (!screen) {
            Serial.println("ERR SHOT NO_SCREEN");
            return;
        }

        uint32_t needed = lv_snapshot_buf_size_needed(screen, LV_IMG_CF_TRUE_COLOR);
        if (needed == 0) {
            Serial.println("ERR SHOT SIZE");
            return;
        }

        uint8_t *buffer = Display::screenBuffer();
        uint32_t capacity = Display::screenBufferSize();
        if (needed > capacity) {
            Serial.printf("ERR SHOT SIZE %u>%u\n",
                          (unsigned)needed, (unsigned)capacity);
            return;
        }

        lv_img_dsc_t snapshot;
        lv_res_t result = lv_snapshot_take_to_buf(screen, LV_IMG_CF_TRUE_COLOR,
                                                  &snapshot, buffer, needed);
        if (result != LV_RES_OK) {
            Serial.println("ERR SHOT SNAPSHOT");
            return;
        }

        Serial.printf("SHOT %u %u RGB565LE %u\n",
                      (unsigned)snapshot.header.w,
                      (unsigned)snapshot.header.h,
                      (unsigned)snapshot.data_size);
        Serial.flush();

        const uint8_t *data = (const uint8_t *)snapshot.data;
        uint32_t total = snapshot.data_size;
        uint32_t sent = 0;
        uint32_t lastProgress = millis();

        while (sent < total) {
            size_t chunk = total - sent;
            if (chunk > 4096) chunk = 4096;
            size_t written = Serial.write(data + sent, chunk);
            if (written == 0) {
                if (millis() - lastProgress > 5000) break;
                delay(1);
            } else {
                sent += (uint32_t)written;
                lastProgress = millis();
            }
        }

        Serial.flush();

        /* The snapshot re-used the real draw buffer; ask LVGL to repaint it
         * on the next refresh. */
        lv_obj_invalidate(lv_scr_act());

        if (sent != total) {
            Serial.printf("ERR SHOT SENT %u/%u\n", (unsigned)sent, (unsigned)total);
        }
    }

    void handlePowerButton()
    {
        bool down = digitalRead(POWER_BUTTON_PIN) == LOW;
        unsigned long now = millis();

        if (down && !buttonWasDown) {
            buttonWasDown = true;
            buttonDownMs = now;
        } else if (!down) {
            buttonWasDown = false;
        } else if (buttonWasDown && (now - buttonDownMs > 2500)) {
            Serial.println("Power off");
            Serial.flush();
            digitalWrite(POWER_LATCH_PIN, LOW);
            while (true) { tight_loop_contents(); }
        }
    }

    void printHelp()
    {
        Serial.println();
        Serial.println("Moon Phase Clock - RP2350-Touch-LCD-3.49");
        Serial.println("Commands:");
        Serial.println("  T YYYY-MM-DD HH:MM:SS   set RTC (local time)");
        Serial.println("  B 0..100                set backlight brightness");
        Serial.println("  F                       flip display 180 degrees");
        Serial.println("  X / Y / S               invert X / invert Y / swap touch axes");
        Serial.println("  C                       capture screenshot (RGB565 over USB)");
        Serial.println("  V                       print display debug info");
        Serial.println("  D / E                   portrait / landscape panel fill test");
        Serial.println("  R                       print current date/time");
        Serial.println("  ?                       this help");
    }

    void handleSerial()
    {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\r') continue;
            if (c == '\n') {
                serialBuf[serialLen] = '\0';
                if (serialLen > 0) {
                    char *line = serialBuf;
                    while (*line == ' ') line++;

                    if (*line == 'T' || *line == 't') {
                        int y, mo, d, h, mi, s;
                        if (sscanf(line + 1, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6 ||
                            sscanf(line + 1, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
                            if (Clock::setDateTime(y, mo, d, h, mi, s)) {
                                Serial.println("RTC set");
                                Astronomy::update(true);
                            } else {
                                Serial.println("Invalid date/time or RTC write failed");
                            }
                        } else {
                            Serial.println("Usage: T 2026-10-17 22:30:00");
                        }
                    } else if (*line == 'B' || *line == 'b') {
                        int percent = atoi(line + 1);
                        if (percent < 0) percent = 0;
                        if (percent > 100) percent = 100;
                        LCD_SetBacklight((uint8_t)percent);
                        Serial.printf("Backlight: %d%%\n", percent);
                    } else if (*line == 'F' || *line == 'f') {
                        displayFlipped = !displayFlipped;
                        Display::setFlipped(displayFlipped);
                        Display::TouchInvertX = !Display::TouchInvertX;
                        Display::TouchInvertY = !Display::TouchInvertY;
                        lv_obj_invalidate(lv_scr_act());
                        lv_refr_now(NULL);
                        Serial.printf("Display rotation: %s\n", displayFlipped ? "flipped" : "normal");
                    } else if (*line == 'X' || *line == 'x') {
                        Display::TouchInvertX = !Display::TouchInvertX;
                        Serial.printf("Touch invert X: %d\n", Display::TouchInvertX);
                    } else if (*line == 'Y' || *line == 'y') {
                        Display::TouchInvertY = !Display::TouchInvertY;
                        Serial.printf("Touch invert Y: %d\n", Display::TouchInvertY);
                    } else if (*line == 'S' || *line == 's') {
                        Display::TouchSwapXY = !Display::TouchSwapXY;
                        Serial.printf("Touch swap XY: %d\n", Display::TouchSwapXY);
                    } else if (*line == 'C' || *line == 'c') {
                        captureScreenshot();
                    } else if (*line == 'V' || *line == 'v') {
                        Serial.println("BUILD partial-rot-20261018");
                        Serial.printf("CLK sys=%u peri=%u\n",
                                      (unsigned)clock_get_hz(clk_sys),
                                      (unsigned)clock_get_hz(clk_peri));
                        Serial.printf("DISP flip=%d touchX=%d touchY=%d swap=%d flush=%u area=%ux%u\n",
                                      Display::flipped() ? 1 : 0,
                                      Display::TouchInvertX ? 1 : 0,
                                      Display::TouchInvertY ? 1 : 0,
                                      Display::TouchSwapXY ? 1 : 0,
                                      (unsigned)Display::flushCount(),
                                      (unsigned)Display::lastFlushWidth(),
                                      (unsigned)Display::lastFlushHeight());
                    } else if (*line == 'D' || *line == 'd') {
                        Display::fillPanelPortrait();
                        delay(5000);
                        lv_obj_invalidate(lv_scr_act());
                        lv_refr_now(NULL);
                    } else if (*line == 'E' || *line == 'e') {
                        Display::fillPanelLandscape();
                        delay(5000);
                        lv_obj_invalidate(lv_scr_act());
                        lv_refr_now(NULL);
                    } else if (*line == 'R' || *line == 'r') {
                        Clock::DateTime dt = Clock::now();
                        Serial.printf("RTC %04d-%02d-%02d %02d:%02d:%02d%s\n",
                                      dt.year, dt.month, dt.day,
                                      dt.hour, dt.minute, dt.second,
                                      Clock::rtcPresent() ? "" : " (software fallback)");
                    } else if (*line == '?' || *line == 'h' || *line == 'H') {
                        printHelp();
                    } else {
                        Serial.println("Unknown command; use ? for help");
                    }
                }
                serialLen = 0;
            } else if (serialLen + 1 < sizeof(serialBuf)) {
                serialBuf[serialLen++] = c;
            }
        }
    }
}

void setup()
{
    /* The board is battery powered through a soft power latch.  Assert it
     * before doing anything else, otherwise the board turns off as soon as
     * the power button is released. */
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, HIGH);

    pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("Moon Phase Clock for RP2350-Touch-LCD-3.49");

    Display::begin();
    ui_init();

    lv_label_set_text(ui_Screen1_Label_bootscreen, "Moon Phase Clock");
    lv_obj_add_flag(ui_Screen1_Container_Container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Screen1_Image_Moon, LV_OBJ_FLAG_HIDDEN);
    moon_Animation(ui_Screen1_Image_bootscreeni, 1000);
    showPlaceholders();

    Clock::begin();
    Astronomy::begin();

    /* Paint one complete frame before turning the backlight on. */
    lv_refr_now(NULL);
    LCD_SetBacklight(defaultBrightness);

    bootStartMs = millis();
    printHelp();
}

void loop()
{
    lv_timer_handler();
    delay(5);

    Clock::update();
    Astronomy::update();
    handleSerial();
    handlePowerButton();

    if (!bootHidden && (millis() - bootStartMs > 1500)) {
        hideBootScreen();
        bootHidden = true;
    }
}
