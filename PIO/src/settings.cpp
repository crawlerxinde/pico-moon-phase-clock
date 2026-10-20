#include "settings.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astronomy.h"
#include "audio_tick.h"
#include "display.h"
#include "battery.h"
#include "brightness.h"
#include "clock.h"
#include "location.h"
#include "moon_intro.h"
#include "settings_store.h"

namespace
{
    enum Field
    {
        FIELD_YEAR = 0,
        FIELD_MONTH,
        FIELD_DAY,
        FIELD_HOUR,
        FIELD_MINUTE,
        FIELD_SECOND,
        FIELD_COUNT
    };

    lv_obj_t *settingsScreen = nullptr;
    lv_obj_t *fieldButton[FIELD_COUNT] = {nullptr};
    lv_obj_t *fieldLabel[FIELD_COUNT] = {nullptr};

    lv_obj_t *batteryDigits = nullptr;
    lv_obj_t *batteryUnit = nullptr;
    lv_obj_t *batteryVolt = nullptr;
    lv_obj_t *batteryStatus = nullptr;
    lv_obj_t *batteryLevel = nullptr;

    lv_obj_t *brightnessSlider = nullptr;
    lv_obj_t *brightnessValueLabel = nullptr;
    lv_obj_t *tickSwitch = nullptr;

    lv_obj_t *locationScreen = nullptr;
    lv_obj_t *locationOffsetLabel = nullptr;
    lv_obj_t *locationCurrentLabel = nullptr;
    lv_obj_t *locationQuickButton[6] = {nullptr};
    int       locationQuickIndex[6] = {0, 1, 2, 3, 4, 5};
    lv_obj_t *locationPickerCityLabel = nullptr;
    lv_obj_t *locationLetterRoller = nullptr;
    lv_obj_t *locationCityRoller = nullptr;
    int       locationCityIndices[256] = {0};
    int       locationCityCount = 0;
    char      locationCityOptions[8192] = {0};
    const char locationLetterOptions[] = "All\nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
    int       locationLetterFilter = -1;
    int       pendingLocationIndex = 0;
    bool      locationOpen = false;
    bool      locationSyncingRollers = false;

    int selectedField = FIELD_MINUTE;
    bool openFlag = false;
    bool controlTouchActive = false;
    bool controlGestureLock = false;
    unsigned long controlTouchReleasedMs = 0;
    unsigned long lastUiRefreshMs = 0;
    unsigned long lastOffsetRefreshMs = 0;

    lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w, int h,
                         const char *text, const lv_font_t *font,
                         uint32_t color, lv_text_align_t align)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_size(label, w, h);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_set_style_text_align(label, align, 0);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
        return label;
    }

    lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h)
    {
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A0A0A), 0);
        lv_obj_set_style_bg_opa(panel, 255, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_radius(panel, 8, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        return panel;
    }

    void control_event_cb(lv_event_t *event)
    {
        lv_event_code_t code = lv_event_get_code(event);
        if (code == LV_EVENT_PRESSED) {
            controlTouchActive = true;
            controlGestureLock = true;
        } else if (code == LV_EVENT_RELEASED ||
                   code == LV_EVENT_PRESS_LOST) {
            controlTouchActive = false;
            controlTouchReleasedMs = millis();
        }
    }

    void guard_control(lv_obj_t *obj)
    {
        lv_obj_add_event_cb(obj, control_event_cb, LV_EVENT_ALL, NULL);
    }

    lv_obj_t *make_button(lv_obj_t *parent, int x, int y, int w, int h,
                          const char *text, const lv_font_t *font,
                          lv_event_cb_t cb, void *userData)
    {
        lv_obj_t *button = lv_obj_create(parent);
        lv_obj_remove_style_all(button);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, w, h);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(button, 255, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0x555555), 0);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_radius(button, 6, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(label);

        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
        return button;
    }

    void updateFieldSelection();

    lv_obj_t *make_field(lv_obj_t *parent, int x, int y, int w, int h,
                         int field)
    {
        lv_obj_t *button = lv_obj_create(parent);
        lv_obj_remove_style_all(button);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, w, h);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(button, 4);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(button, 0, 0);
        lv_obj_set_style_radius(button, 4, 0);
        lv_obj_set_style_border_width(button, 0, 0);

        lv_obj_t *label = lv_label_create(button);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_label_set_text(label, "");
        lv_obj_set_style_text_font(label, &ui_font_Inter18, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

        lv_obj_add_event_cb(button, [](lv_event_t *event) {
            selectedField = (int)(intptr_t)lv_event_get_user_data(event);
            updateFieldSelection();
        }, LV_EVENT_CLICKED, (void *)(intptr_t)field);

        fieldButton[field] = button;
        fieldLabel[field] = label;
        return button;
    }

    void updateFieldSelection()
    {
        for (int i = 0; i < FIELD_COUNT; i++) {
            bool selected = (i == selectedField);
            lv_obj_set_style_bg_opa(fieldButton[i], selected ? 255 : 0, 0);
            lv_obj_set_style_bg_color(fieldButton[i], lv_color_hex(0x333333), 0);
            lv_obj_set_style_border_width(fieldButton[i], selected ? 1 : 0, 0);
            lv_obj_set_style_border_color(fieldButton[i], lv_color_hex(0x666666), 0);
        }
    }

    void formatOffset(int offsetSec, char *out, size_t outSize)
    {
        int sign = offsetSec < 0 ? -1 : 1;
        int absOffset = abs(offsetSec);
        int hours = absOffset / 3600;
        int minutes = (absOffset % 3600) / 60;
        if (minutes == 0) {
            snprintf(out, outSize, "UTC%c%d", sign < 0 ? '-' : '+', hours);
        } else {
            snprintf(out, outSize, "UTC%c%d:%02d", sign < 0 ? '-' : '+',
                     hours, minutes);
        }
    }

    void refreshBattery()
    {
        Battery::update();

        if (Battery::present()) {
            char digits[8];
            snprintf(digits, sizeof(digits), "%d", Battery::percent());
            lv_label_set_text(batteryDigits, digits);
            lv_obj_set_style_text_font(batteryDigits, &ui_font_Inter66, 0);
            lv_obj_set_pos(batteryDigits, 8, 18);
            lv_obj_set_size(batteryDigits, 118, 68);
            lv_obj_clear_flag(batteryUnit, LV_OBJ_FLAG_HIDDEN);

            int width = (int)(148.0f * (float)Battery::percent() / 100.0f);
            if (width < 4 && Battery::percent() > 0) width = 4;
            lv_obj_set_width(batteryLevel, width);
            if (width > 0) {
                lv_obj_clear_flag(batteryLevel, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(batteryLevel, LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_set_style_bg_color(batteryLevel, lv_color_hex(0xFFFFFF), 0);

            int millivolts = (int)(Battery::voltage() * 1000.0f + 0.5f);
            char voltage[20];
            snprintf(voltage, sizeof(voltage), "%d.%02d V",
                     millivolts / 1000, (millivolts % 1000) / 10);
            lv_label_set_text(batteryVolt, voltage);
            lv_label_set_text(batteryStatus, "Battery");
        } else {
            lv_label_set_text(batteryDigits, "USB");
            lv_obj_set_style_text_font(batteryDigits, &ui_font_Inter18, 0);
            lv_obj_set_pos(batteryDigits, 8, 48);
            lv_obj_set_size(batteryDigits, 118, 28);
            lv_obj_add_flag(batteryUnit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(batteryLevel, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(batteryVolt, "No battery");
            lv_label_set_text(batteryStatus, "External power");
        }
    }

    void refreshDateTime()
    {
        Clock::DateTime dt = Clock::now();
        char buf[8];

        snprintf(buf, sizeof(buf), "%04d", dt.year);
        lv_label_set_text(fieldLabel[FIELD_YEAR], buf);
        snprintf(buf, sizeof(buf), "%02d", dt.month);
        lv_label_set_text(fieldLabel[FIELD_MONTH], buf);
        snprintf(buf, sizeof(buf), "%02d", dt.day);
        lv_label_set_text(fieldLabel[FIELD_DAY], buf);
        snprintf(buf, sizeof(buf), "%02d", dt.hour);
        lv_label_set_text(fieldLabel[FIELD_HOUR], buf);
        snprintf(buf, sizeof(buf), "%02d", dt.minute);
        lv_label_set_text(fieldLabel[FIELD_MINUTE], buf);
        snprintf(buf, sizeof(buf), "%02d", dt.second);
        lv_label_set_text(fieldLabel[FIELD_SECOND], buf);
    }

    void updateLocationOffset(int index)
    {
        Clock::DateTime dt = Clock::now();
        char offset[12];
        formatOffset(Location::offsetForIndex(index, dt.year, dt.month, dt.day),
                     offset, sizeof(offset));
        lv_label_set_text(locationOffsetLabel, offset);
    }

    char locationInitial(int index)
    {
        const char *name = Location::at(index).name;
        if (!name || !name[0]) return '\0';
        char c = name[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        return c;
    }

    bool locationMatchesFilter(int index)
    {
        if (locationLetterFilter < 0) return true;
        return locationInitial(index) == (char)('A' + locationLetterFilter);
    }

    void updateLocationPickerSummary(int index)
    {
        if (!locationPickerCityLabel) return;

        Clock::DateTime dt = Clock::now();
        char offset[12];
        formatOffset(Location::offsetForIndex(index, dt.year, dt.month, dt.day),
                     offset, sizeof(offset));

        char text[64];
        snprintf(text, sizeof(text), "%s  %s",
                 Location::at(index).name, offset);
        lv_label_set_text(locationPickerCityLabel, text);
    }

    void styleLocationRoller(lv_obj_t *roller, lv_text_align_t align,
                             lv_coord_t padLeft)
    {
        lv_obj_set_style_bg_opa(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(roller, padLeft, LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, &ui_font_Inter18, LV_PART_MAIN);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xAAAAAA),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_align(roller, align, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(roller, 0, LV_PART_MAIN);

        lv_obj_set_style_bg_color(roller, lv_color_hex(0x333333),
                                  LV_PART_SELECTED);
        lv_obj_set_style_bg_opa(roller, 255, LV_PART_SELECTED);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xFFFFFF),
                                    LV_PART_SELECTED);
        lv_obj_set_style_radius(roller, 6, LV_PART_SELECTED);
    }

    void rebuildCityRollerOptions()
    {
        if (!locationCityRoller) return;

        locationCityCount = 0;
        locationCityOptions[0] = '\0';

        for (int i = 0; i < Location::count() && i < 256; i++) {
            if (locationMatchesFilter(i)) {
                locationCityIndices[locationCityCount++] = i;
            }
        }

        if (locationCityCount == 0) {
            snprintf(locationCityOptions, sizeof(locationCityOptions), "--");
        } else {
            /* The source table keeps the original six cities first.  Sort the
             * filtered roller alphabetically so the selected city sits in its
             * natural place instead of at the top of its letter group. */
            for (int i = 1; i < locationCityCount; i++) {
                int key = locationCityIndices[i];
                int j = i - 1;
                while (j >= 0 &&
                       strcmp(Location::at(locationCityIndices[j]).name,
                              Location::at(key).name) > 0) {
                    locationCityIndices[j + 1] = locationCityIndices[j];
                    j--;
                }
                locationCityIndices[j + 1] = key;
            }

            size_t used = 0;
            locationCityOptions[0] = '\0';
            for (int i = 0; i < locationCityCount; i++) {
                int written = snprintf(
                    locationCityOptions + used,
                    sizeof(locationCityOptions) - used,
                    "%s%s", i ? "\n" : "",
                    Location::at(locationCityIndices[i]).name);
                if (written < 0 ||
                    (size_t)written >= sizeof(locationCityOptions) - used) {
                    locationCityCount = i;
                    break;
                }
                used += (size_t)written;
            }
        }
        lv_roller_set_options(locationCityRoller, locationCityOptions,
                              LV_ROLLER_MODE_NORMAL);
    }

    void syncCityRollerToPending()
    {
        if (!locationCityRoller) return;

        int target = 0;
        bool found = false;
        for (int i = 0; i < locationCityCount; i++) {
            if (locationCityIndices[i] == pendingLocationIndex) {
                target = i;
                found = true;
                break;
            }
        }

        if (!found && locationCityCount > 0) {
            pendingLocationIndex = locationCityIndices[0];
            target = 0;
        }

        if (locationCityCount > 0) {
            locationSyncingRollers = true;
            lv_roller_set_selected(locationCityRoller, (uint16_t)target,
                                   LV_ANIM_OFF);
            locationSyncingRollers = false;
        }

        updateLocationPickerSummary(pendingLocationIndex);
    }

    void refreshLocationPicker()
    {
        if (!locationLetterRoller || !locationCityRoller) return;

        int letterIndex = locationLetterFilter + 1;
        if (letterIndex < 0) letterIndex = 0;
        if (letterIndex > 26) letterIndex = 26;

        locationSyncingRollers = true;
        lv_roller_set_selected(locationLetterRoller, (uint16_t)letterIndex,
                               LV_ANIM_OFF);
        rebuildCityRollerOptions();
        locationSyncingRollers = false;

        syncCityRollerToPending();
    }

    void onLocationLetterChanged(lv_event_t *event)
    {
        if (locationSyncingRollers) return;

        int selected = (int)lv_roller_get_selected(lv_event_get_target(event));
        locationLetterFilter = selected - 1;

        locationSyncingRollers = true;
        rebuildCityRollerOptions();
        locationSyncingRollers = false;

        syncCityRollerToPending();
    }

    void onLocationCityChanged(lv_event_t *event)
    {
        if (locationSyncingRollers) return;

        int selected = (int)lv_roller_get_selected(lv_event_get_target(event));
        if (selected >= 0 && selected < locationCityCount) {
            pendingLocationIndex = locationCityIndices[selected];
            updateLocationPickerSummary(pendingLocationIndex);
        }
    }

    void refreshSettingsLocation()
    {
        if (!locationCurrentLabel) return;

        lv_label_set_text(locationCurrentLabel, Location::city());
        updateLocationOffset(Location::index());

        for (int i = 0; i < 6; i++) {
            if (!locationQuickButton[i]) continue;
            bool selected = (locationQuickIndex[i] == Location::index());
            lv_obj_set_style_bg_color(
                locationQuickButton[i],
                lv_color_hex(selected ? 0x333333 : 0x1A1A1A), 0);
            lv_obj_set_style_border_color(
                locationQuickButton[i],
                lv_color_hex(selected ? 0x888888 : 0x555555), 0);
        }
    }

    void refreshLocation()
    {
        refreshSettingsLocation();
        if (locationOpen) refreshLocationPicker();
    }

    void refreshBrightnessControl()
    {
        if (!brightnessSlider) return;
        int value = Brightness::current();
        if (lv_slider_get_value(brightnessSlider) != value) {
            lv_slider_set_value(brightnessSlider, value, LV_ANIM_OFF);
        }
        char text[8];
        snprintf(text, sizeof(text), "%d%%", value);
        lv_label_set_text(brightnessValueLabel, text);
    }

    void refreshAll()
    {
        refreshDateTime();
        refreshBattery();
        refreshLocation();
        refreshBrightnessControl();
        lastUiRefreshMs = millis();
        lastOffsetRefreshMs = millis();
    }

    void adjustSelected(int delta)
    {
        Clock::DateTime dt = Clock::now();
        int year = dt.year;
        int month = dt.month;
        int day = dt.day;
        int hour = dt.hour;
        int minute = dt.minute;
        int second = dt.second;

        switch (selectedField) {
        case FIELD_YEAR:
            year += delta;
            if (year < 2020) year = 2020;
            if (year > 2069) year = 2069;
            break;

        case FIELD_MONTH:
            month += delta;
            if (month < 1) month = 12;
            if (month > 12) month = 1;
            break;

        case FIELD_DAY:
            day += delta;
            if (day < 1) {
                month--;
                if (month < 1) {
                    month = 12;
                    year--;
                    if (year < 2020) {
                        year = 2020;
                        month = 1;
                    }
                }
                day = Clock::daysInMonth(year, month);
            } else if (day > Clock::daysInMonth(year, month)) {
                day = 1;
                month++;
                if (month > 12) {
                    month = 1;
                    year++;
                    if (year > 2069) {
                        year = 2069;
                        month = 12;
                        day = 31;
                    }
                }
            }
            break;

        case FIELD_HOUR:
            hour = (hour + delta + 24) % 24;
            break;

        case FIELD_MINUTE:
            minute = (minute + delta + 60) % 60;
            break;

        case FIELD_SECOND:
            second = (second + delta + 60) % 60;
            break;
        }

        if (day > Clock::daysInMonth(year, month)) {
            day = Clock::daysInMonth(year, month);
        }

        if (Clock::setDateTime(year, month, day, hour, minute, second)) {
            SettingsStore::save();
            if (selectedField <= FIELD_DAY) {
                Astronomy::update(true);
            } else {
                /* The rise/set times only change at midnight, but the moon
                 * orientation follows the current hour angle immediately. */
                Astronomy::updateOrientation();
            }
            refreshDateTime();
            refreshLocation();
        }
    }

}

namespace Settings
{
    void begin()
    {
        if (settingsScreen) return;

        settingsScreen = lv_obj_create(NULL);
        lv_obj_clear_flag(settingsScreen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(settingsScreen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(settingsScreen, 255, 0);
        make_label(settingsScreen, 12, 5, 120, 22, "Settings",
                   &ui_font_Inter18, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        /* Transparent touch guard around the header controls.  A finger that
         * starts in this band is treated as adjusting BRIGHT/TICK, not as a
         * swipe back to the clock.  The slider/switch are created afterwards
         * and stay on top, so they still receive their own touches. */
        lv_obj_t *headerGuard = lv_obj_create(settingsScreen);
        lv_obj_remove_style_all(headerGuard);
        lv_obj_set_pos(headerGuard, 190, 0);
        lv_obj_set_size(headerGuard, 230, 48);
        lv_obj_add_flag(headerGuard, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(headerGuard, LV_OBJ_FLAG_SCROLLABLE);
        guard_control(headerGuard);

        make_label(settingsScreen, 140, 8, 58, 16, "BRIGHT",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);

        brightnessSlider = lv_slider_create(settingsScreen);
        lv_obj_set_pos(brightnessSlider, 200, 9);
        lv_obj_set_size(brightnessSlider, 80, 14);
        lv_slider_set_range(brightnessSlider, 40, 100);
        lv_slider_set_value(brightnessSlider, Brightness::current(), LV_ANIM_OFF);
        lv_obj_set_ext_click_area(brightnessSlider, 10);
        lv_obj_set_style_bg_color(brightnessSlider, lv_color_hex(0x333333),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(brightnessSlider, 255, LV_PART_MAIN);
        lv_obj_set_style_radius(brightnessSlider, 7, LV_PART_MAIN);
        lv_obj_set_style_border_width(brightnessSlider, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(brightnessSlider, lv_color_hex(0xBBBBBB),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(brightnessSlider, 255, LV_PART_INDICATOR);
        lv_obj_set_style_radius(brightnessSlider, 7, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(brightnessSlider, lv_color_hex(0xFFFFFF),
                                  LV_PART_KNOB);
        lv_obj_set_style_bg_opa(brightnessSlider, 255, LV_PART_KNOB);
        lv_obj_set_style_pad_all(brightnessSlider, 0, LV_PART_KNOB);
        lv_obj_set_style_radius(brightnessSlider, LV_RADIUS_CIRCLE,
                                LV_PART_KNOB);
        guard_control(brightnessSlider);
        lv_obj_add_event_cb(brightnessSlider, [](lv_event_t *event) {
            lv_obj_t *slider = lv_event_get_target(event);
            int value = lv_slider_get_value(slider);
            Brightness::set(value);
            char text[8];
            snprintf(text, sizeof(text), "%d%%", Brightness::current());
            lv_label_set_text(brightnessValueLabel, text);
        }, LV_EVENT_VALUE_CHANGED, NULL);

        brightnessValueLabel = make_label(settingsScreen, 284, 8, 36, 16, "100%",
                                          &ui_font_Inter10M, 0xCCCCCC,
                                          LV_TEXT_ALIGN_RIGHT);

        make_label(settingsScreen, 328, 8, 30, 16, "TICK",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);
        tickSwitch = lv_switch_create(settingsScreen);
        lv_obj_set_pos(tickSwitch, 362, 7);
        lv_obj_set_size(tickSwitch, 36, 18);
        lv_obj_set_ext_click_area(tickSwitch, 8);
        lv_obj_set_style_bg_color(tickSwitch, lv_color_hex(0x333333),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tickSwitch, 255, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tickSwitch, lv_color_hex(0xBBBBBB),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(tickSwitch, 255, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(tickSwitch, lv_color_hex(0xBBBBBB),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(tickSwitch, 255,
                                LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(tickSwitch, lv_color_hex(0xFFFFFF),
                                  LV_PART_KNOB);
        lv_obj_set_style_bg_opa(tickSwitch, 255, LV_PART_KNOB);
        lv_obj_set_style_pad_all(tickSwitch, 0, LV_PART_KNOB);
        if (Audio::enabled()) {
            lv_obj_add_state(tickSwitch, LV_STATE_CHECKED);
        }
        guard_control(tickSwitch);
        lv_obj_add_event_cb(tickSwitch, [](lv_event_t *event) {
            bool on = lv_obj_has_state(lv_event_get_target(event),
                                       LV_STATE_CHECKED);
            Audio::setEnabled(on);
            SettingsStore::save();
        }, LV_EVENT_VALUE_CHANGED, NULL);

        make_label(settingsScreen, 410, 8, 218, 16, "Swipe left column to return",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *headerLine = lv_obj_create(settingsScreen);
        lv_obj_remove_style_all(headerLine);
        lv_obj_set_pos(headerLine, 12, 29);
        lv_obj_set_size(headerLine, 616, 1);
        lv_obj_set_style_bg_color(headerLine, lv_color_hex(0x444444), 0);
        lv_obj_set_style_bg_opa(headerLine, 255, 0);
        lv_obj_clear_flag(headerLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        /* Battery card. */
        lv_obj_t *batteryPanel = make_panel(settingsScreen, 12, 36, 164, 130);
        make_label(batteryPanel, 8, 6, 140, 14, "BATTERY",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);

        batteryDigits = make_label(batteryPanel, 8, 20, 118, 68, "0",
                                   &ui_font_Inter66, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        batteryUnit = make_label(batteryPanel, 126, 62, 24, 24, "%",
                                 &ui_font_Inter18, 0xAAAAAA, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *batteryBar = lv_obj_create(batteryPanel);
        lv_obj_remove_style_all(batteryBar);
        lv_obj_set_pos(batteryBar, 8, 86);
        lv_obj_set_size(batteryBar, 148, 8);
        lv_obj_set_style_bg_color(batteryBar, lv_color_hex(0x222222), 0);
        lv_obj_set_style_bg_opa(batteryBar, 255, 0);
        lv_obj_set_style_radius(batteryBar, 4, 0);
        lv_obj_clear_flag(batteryBar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        batteryLevel = lv_obj_create(batteryPanel);
        lv_obj_remove_style_all(batteryLevel);
        lv_obj_set_pos(batteryLevel, 8, 86);
        lv_obj_set_size(batteryLevel, 0, 8);
        lv_obj_set_style_bg_color(batteryLevel, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(batteryLevel, 255, 0);
        lv_obj_set_style_radius(batteryLevel, 4, 0);
        lv_obj_clear_flag(batteryLevel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        batteryVolt = make_label(batteryPanel, 8, 98, 148, 14, "--",
                                 &ui_font_Inter11R, 0xAAAAAA, LV_TEXT_ALIGN_LEFT);
        batteryStatus = make_label(batteryPanel, 8, 112, 148, 12, "",
                                   &ui_font_Inter10M, 0x777777, LV_TEXT_ALIGN_LEFT);

        /* Date/time card. */
        lv_obj_t *timePanel = make_panel(settingsScreen, 184, 36, 258, 130);

        make_label(timePanel, 10, 4, 60, 14, "DATE",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);
        make_label(timePanel, 10, 50, 60, 14, "TIME",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);

        make_field(timePanel, 10, 18, 58, 28, FIELD_YEAR);
        make_label(timePanel, 68, 18, 10, 28, "-",
                   &ui_font_Inter18, 0x666666, LV_TEXT_ALIGN_CENTER);
        make_field(timePanel, 78, 18, 36, 28, FIELD_MONTH);
        make_label(timePanel, 114, 18, 10, 28, "-",
                   &ui_font_Inter18, 0x666666, LV_TEXT_ALIGN_CENTER);
        make_field(timePanel, 124, 18, 36, 28, FIELD_DAY);

        make_field(timePanel, 10, 62, 38, 28, FIELD_HOUR);
        make_label(timePanel, 48, 62, 10, 28, ":",
                   &ui_font_Inter18, 0x666666, LV_TEXT_ALIGN_CENTER);
        make_field(timePanel, 58, 62, 38, 28, FIELD_MINUTE);
        make_label(timePanel, 96, 62, 10, 28, ":",
                   &ui_font_Inter18, 0x666666, LV_TEXT_ALIGN_CENTER);
        make_field(timePanel, 106, 62, 38, 28, FIELD_SECOND);
        updateFieldSelection();

        make_button(timePanel, 10, 96, 42, 24, "-",
                    &ui_font_Inter18, [](lv_event_t *) { adjustSelected(-1); }, NULL);
        make_button(timePanel, 58, 96, 42, 24, "+",
                    &ui_font_Inter18, [](lv_event_t *) { adjustSelected(1); }, NULL);
        make_label(timePanel, 110, 99, 138, 18, "Tap field, then -/+",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);

        /* Location card: current city + quick cities + full picker. */
        lv_obj_t *locationPanel = make_panel(settingsScreen, 450, 36, 178, 130);
        make_label(locationPanel, 8, 6, 72, 14, "LOCATION",
                   &ui_font_Inter10M, 0x888888, LV_TEXT_ALIGN_LEFT);
        locationOffsetLabel = make_label(locationPanel, 86, 6, 84, 14, "",
                                         &ui_font_Inter10M, 0x888888,
                                         LV_TEXT_ALIGN_RIGHT);

        locationCurrentLabel = make_label(locationPanel, 8, 20, 160, 18, "",
                                          &ui_font_Inter15SB, 0xFFFFFF,
                                          LV_TEXT_ALIGN_LEFT);
        lv_obj_add_flag(locationCurrentLabel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(locationCurrentLabel, [](lv_event_t *) {
            Settings::showLocationPicker();
        }, LV_EVENT_CLICKED, NULL);

        const int quickX[2] = {8, 92};
        const int quickY[3] = {40, 61, 82};
        for (int i = 0; i < 6; i++) {
            const int index = i;
            locationQuickIndex[i] = index;
            locationQuickButton[i] = make_button(
                locationPanel, quickX[i % 2], quickY[i / 2], 78, 20,
                Location::at(index).name, &ui_font_Inter11R,
                [](lv_event_t *event) {
                    Settings::selectLocation(
                        (int)(intptr_t)lv_event_get_user_data(event));
                }, (void *)(intptr_t)index);
        }
        make_button(locationPanel, 8, 104, 160, 22,
                    "All cities", &ui_font_Inter11R,
                    [](lv_event_t *) { Settings::showLocationPicker(); }, NULL);

        /* Full-screen city picker, opened from the Location card. */
        locationScreen = lv_obj_create(NULL);
        lv_obj_clear_flag(locationScreen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(locationScreen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(locationScreen, 255, 0);

        make_label(locationScreen, 12, 4, 100, 24, "LOCATION",
                   &ui_font_Inter18, 0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        locationPickerCityLabel = make_label(locationScreen, 120, 5, 500, 22,
                                              "", &ui_font_Inter15M,
                                              0xCCCCCC, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *locationHeaderLine = lv_obj_create(locationScreen);
        lv_obj_remove_style_all(locationHeaderLine);
        lv_obj_set_pos(locationHeaderLine, 12, 34);
        lv_obj_set_size(locationHeaderLine, 616, 1);
        lv_obj_set_style_bg_color(locationHeaderLine, lv_color_hex(0x444444), 0);
        lv_obj_set_style_bg_opa(locationHeaderLine, 255, 0);
        lv_obj_clear_flag(locationHeaderLine,
                          LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        make_label(locationScreen, 12, 36, 96, 10, "A-Z",
                   &ui_font_Inter10M, 0x777777, LV_TEXT_ALIGN_LEFT);
        make_label(locationScreen, 120, 36, 200, 10, "CITIES",
                   &ui_font_Inter10M, 0x777777, LV_TEXT_ALIGN_LEFT);
        make_label(locationScreen, 542, 36, 86, 10, "ACTION",
                   &ui_font_Inter10M, 0x777777, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *locationDivider = lv_obj_create(locationScreen);
        lv_obj_remove_style_all(locationDivider);
        lv_obj_set_pos(locationDivider, 112, 48);
        lv_obj_set_size(locationDivider, 1, 108);
        lv_obj_set_style_bg_color(locationDivider, lv_color_hex(0x333333), 0);
        lv_obj_set_style_bg_opa(locationDivider, 255, 0);
        lv_obj_clear_flag(locationDivider,
                          LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *locationActionDivider = lv_obj_create(locationScreen);
        lv_obj_remove_style_all(locationActionDivider);
        lv_obj_set_pos(locationActionDivider, 530, 48);
        lv_obj_set_size(locationActionDivider, 1, 108);
        lv_obj_set_style_bg_color(locationActionDivider,
                                  lv_color_hex(0x333333), 0);
        lv_obj_set_style_bg_opa(locationActionDivider, 255, 0);
        lv_obj_clear_flag(locationActionDivider,
                          LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        locationLetterRoller = lv_roller_create(locationScreen);
        styleLocationRoller(locationLetterRoller, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(locationLetterRoller, 12, 48);
        lv_obj_set_width(locationLetterRoller, 96);
        lv_roller_set_options(locationLetterRoller, locationLetterOptions,
                              LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationLetterRoller, 6);
        lv_obj_add_event_cb(locationLetterRoller, onLocationLetterChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);

        locationCityRoller = lv_roller_create(locationScreen);
        styleLocationRoller(locationCityRoller, LV_TEXT_ALIGN_LEFT, 14);
        lv_obj_set_pos(locationCityRoller, 120, 48);
        lv_obj_set_width(locationCityRoller, 406);
        lv_roller_set_options(locationCityRoller, "", LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationCityRoller, 6);
        lv_obj_add_event_cb(locationCityRoller, onLocationCityChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);

        make_button(locationScreen, 542, 66, 86, 32, "Cancel",
                    &ui_font_Inter11R, [](lv_event_t *) {
                        Settings::hideLocationPicker();
                    }, NULL);
        make_button(locationScreen, 542, 106, 86, 32, "Apply",
                    &ui_font_Inter11R, [](lv_event_t *) {
                        Settings::selectLocation(pendingLocationIndex);
                        Settings::hideLocationPicker();
                    }, NULL);

        /* Tap the location pill as a discoverable alternative to swiping. */
        lv_obj_add_flag(ui_Screen1_Panel_border, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_Screen1_Panel_border, [](lv_event_t *) {
            Settings::show();
        }, LV_EVENT_CLICKED, NULL);

        refreshAll();
    }

    void update()
    {
        if (!openFlag || locationOpen) return;

        unsigned long nowMs = millis();
        if (nowMs - lastUiRefreshMs >= 250) {
            refreshDateTime();
            refreshBattery();
            refreshBrightnessControl();
            lastUiRefreshMs = nowMs;
        }

        if (nowMs - lastOffsetRefreshMs >= 60000UL) {
            refreshLocation();
            lastOffsetRefreshMs = nowMs;
        }

    }

    static lv_scr_load_anim_t swipeAnimation(int swipeDx,
                                              lv_scr_load_anim_t fallback)
    {
        if (swipeDx < 0) return LV_SCR_LOAD_ANIM_MOVE_LEFT;
        if (swipeDx > 0) return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        return fallback;
    }

    void show(int swipeDx)
    {
        if (!settingsScreen || openFlag) return;
        openFlag = true;
        locationOpen = false;
        refreshAll();
        lv_scr_load_anim(settingsScreen,
                         swipeAnimation(swipeDx, LV_SCR_LOAD_ANIM_MOVE_LEFT),
                         150, 0, false);
    }

    void hide(int swipeDx)
    {
        if (!settingsScreen || !openFlag) return;
        openFlag = false;
        locationOpen = false;
        SettingsStore::save();
        lv_scr_load_anim(ui_Screen1,
                         swipeAnimation(swipeDx, LV_SCR_LOAD_ANIM_MOVE_RIGHT),
                         150, 0, false);
        MoonIntro::play();
    }

    bool isOpen()
    {
        return openFlag;
    }

    void showLocationPicker()
    {
        if (!locationScreen || locationOpen) return;

        locationOpen = true;
        pendingLocationIndex = Location::index();

        char initial = locationInitial(pendingLocationIndex);
        if (initial >= 'A' && initial <= 'Z') {
            locationLetterFilter = initial - 'A';
        } else {
            locationLetterFilter = -1;
        }

        refreshLocationPicker();
        lv_scr_load_anim(locationScreen,
                         LV_SCR_LOAD_ANIM_MOVE_LEFT, 150, 0, false);
    }

    void hideLocationPicker(int swipeDx)
    {
        if (!locationOpen) return;

        locationOpen = false;
        refreshAll();
        lv_scr_load_anim(settingsScreen,
                         swipeAnimation(swipeDx, LV_SCR_LOAD_ANIM_MOVE_RIGHT),
                         150, 0, false);
    }

    bool isLocationPickerOpen()
    {
        return locationOpen;
    }

    bool swipeBlocked()
    {
        if (controlTouchActive) return true;

        if (controlGestureLock) {
            /* Keep the lock for the rest of the physical touch, even if the
             * slider sends PRESS_LOST when the finger leaves its bounds. */
            if (Display::TouchDown) return true;
            if (controlTouchReleasedMs != 0 &&
                (millis() - controlTouchReleasedMs) < 300) {
                return true;
            }
            controlGestureLock = false;
        }
        return false;
    }

    void adjustField(int field, int delta)
    {
        if (field < 0 || field >= FIELD_COUNT) return;
        selectedField = field;
        updateFieldSelection();
        adjustSelected(delta);
    }

    void selectLocation(int index)
    {
        if (index < 0 || index >= Location::count()) return;
        if (index == Location::index()) return;

        Clock::DateTime now = Clock::now();
        int oldOffset = Location::offsetForDate(now.year, now.month, now.day);
        time_t utc = Clock::localEpoch() - (time_t)oldOffset;

        if (!Location::setIndex(index)) return;

        pendingLocationIndex = index;
        int newOffset = Location::offsetForUtc(utc);
        Clock::setFromUtc(utc, newOffset);
        SettingsStore::save();
        Astronomy::update(true);

        if (openFlag) refreshAll();
    }
}
