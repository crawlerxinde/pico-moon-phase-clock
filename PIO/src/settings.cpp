#include "settings.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astronomy.h"
#include "audio_tick.h"
#include "brightness.h"
#include "clock.h"
#include "location.h"
#include "moon_intro.h"
#include "settings_store.h"

namespace
{
    enum Page
    {
        PAGE_OVERVIEW,
        PAGE_DATE,
        PAGE_TIME,
        PAGE_REVIEW,
        PAGE_LOCATION,
        PAGE_TICK,
        PAGE_BRIGHTNESS,
        PAGE_CONFIRM
    };

    enum PendingAction
    {
        ACTION_DATETIME,
        ACTION_LOCATION
    };

    enum ConfirmMode
    {
        CONFIRM_APPLY,
        CONFIRM_DISCARD
    };

    constexpr uint32_t COLOR_BG          = 0x0B0E12;
    constexpr uint32_t COLOR_CARD        = 0x1A2027;
    constexpr uint32_t COLOR_CARD_PRESS  = 0x252D37;
    constexpr uint32_t COLOR_LINE        = 0x39424D;
    constexpr uint32_t COLOR_TEXT        = 0xFFFFFF;
    constexpr uint32_t COLOR_MUTED       = 0x8D97A5;
    constexpr uint32_t COLOR_BLUE        = 0x087BFF;
    constexpr uint32_t COLOR_BLUE_EDGE   = 0x63A9FF;
    constexpr uint32_t COLOR_PINK        = 0xB06A8A;

    bool openFlag = false;
    Page currentPage = PAGE_OVERVIEW;
    PendingAction pendingAction = ACTION_DATETIME;
    ConfirmMode confirmMode = CONFIRM_APPLY;
    Page confirmReturnPage = PAGE_OVERVIEW;
    Page discardSourcePage = PAGE_OVERVIEW;
    int overviewFocus = 0;
    bool saving = false;
    unsigned long lastRefreshMs = 0;

    Clock::DateTime draftDateTime = {2026, 1, 1, 0, 0, 0, 0};
    Clock::DateTime originalDateTime = {2026, 1, 1, 0, 0, 0, 0};
    int timeStep = 0; /* 0 = hour, 1 = minute */

    int draftCity = 0;
    int originalCity = 0;

    lv_obj_t *overviewScreen = nullptr;
    lv_obj_t *dateScreen = nullptr;
    lv_obj_t *timeScreen = nullptr;
    lv_obj_t *reviewScreen = nullptr;
    lv_obj_t *locationScreen = nullptr;
    lv_obj_t *tickScreen = nullptr;
    lv_obj_t *brightnessScreen = nullptr;
    lv_obj_t *confirmScreen = nullptr;

    lv_obj_t *overviewCards[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t *overviewDate = nullptr;
    lv_obj_t *overviewTime = nullptr;
    lv_obj_t *overviewCity = nullptr;
    lv_obj_t *overviewOffset = nullptr;
    lv_obj_t *overviewTick = nullptr;
    lv_obj_t *overviewTickState = nullptr;
    lv_obj_t *overviewBrightness = nullptr;

    lv_obj_t *dateRoller[3] = {nullptr, nullptr, nullptr};
    bool dateSyncing = false;

    lv_obj_t *timeTitle = nullptr;
    lv_obj_t *timeFieldLabel = nullptr;
    lv_obj_t *timeValueLabel = nullptr;

    lv_obj_t *reviewDateLabel = nullptr;
    lv_obj_t *reviewTimeLabel = nullptr;

    lv_obj_t *locationLetterRoller = nullptr;
    lv_obj_t *locationCityRoller = nullptr;
    lv_obj_t *locationOffsetLabel = nullptr;
    lv_obj_t *locationApply = nullptr;
    const char locationLetterOptions[] = "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
    char locationCityOptions[4096] = {0};
    int locationCityIndices[256] = {0};
    int locationCityCount = 0;
    int locationLetterFilter = 0;
    int lastCityForLetter[26];
    bool locationSyncing = false;

    lv_obj_t *tickStateIcon = nullptr;
    lv_obj_t *tickState = nullptr;
    lv_obj_t *tickDetail = nullptr;
    lv_obj_t *tickAction = nullptr;
    lv_obj_t *tickActionLabel = nullptr;
    lv_obj_t *tickActionSub = nullptr;
    lv_obj_t *tickActionIcon = nullptr;

    lv_obj_t *brightnessFill = nullptr;
    lv_obj_t *brightnessValue = nullptr;

    lv_obj_t *confirmIcon = nullptr;
    lv_obj_t *confirmTitle = nullptr;
    lv_obj_t *confirmMessage = nullptr;
    lv_obj_t *confirmSub = nullptr;
    lv_obj_t *confirmActionLabel = nullptr;

    void showOverview();
    void showDateScreen(bool reset);
    void showTimeScreen(bool hourStep);
    void showReview();
    void showLocationScreen(bool reset);
    void showTickScreen();
    void showBrightnessScreen();
    void showConfirmApply(PendingAction action);
    void showConfirmDiscard(Page source, Page returnPage);

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

    lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h,
                        bool clickable = false)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_remove_style_all(card);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, w, h);
        lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), 0);
        lv_obj_set_style_bg_opa(card, 255, 0);
        lv_obj_set_style_radius(card, 14, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        if (clickable) {
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_PRESS),
                                      LV_STATE_PRESSED);
        }
        return card;
    }

    lv_obj_t *make_action_card(lv_obj_t *parent, int x, int y, int w, int h,
                               const char *text, const lv_font_t *font,
                               uint32_t bg, uint32_t border, uint32_t fg,
                               lv_event_cb_t cb)
    {
        lv_obj_t *card = make_card(parent, x, y, w, h, true);
        lv_obj_set_style_bg_color(card, lv_color_hex(bg), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(border), 0);
        lv_obj_set_style_border_width(card, border ? 2 : 0, 0);
        lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(card);
        lv_obj_set_width(label, w);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        return card;
    }

    lv_obj_t *make_back_card(lv_obj_t *parent, int x, int y, int w, int h,
                             lv_event_cb_t cb)
    {
        lv_obj_t *card = make_card(parent, x, y, w, h, true);
        lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
        make_label(card, 0, 12, w, 28, "\xEF\x83\xA2", &ui_font_FA28,
                   COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *line = lv_obj_create(card);
        lv_obj_remove_style_all(line);
        lv_obj_set_pos(line, 12, 52);
        lv_obj_set_size(line, w - 24, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(line, 255, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        make_label(card, 0, 62, w, 24, "Back", &ui_font_Inter18,
                   COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        return card;
    }

    lv_obj_t *make_primary_card(lv_obj_t *parent, int x, int y, int w, int h,
                                const char *icon, const char *text,
                                lv_event_cb_t cb)
    {
        lv_obj_t *card = make_card(parent, x, y, w, h, true);
        lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_BLUE), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2E9BFF),
                                  LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
        if (icon && icon[0]) {
            make_label(card, 0, 12, w, 28, icon, &ui_font_FA28,
                       COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        }
        lv_obj_t *line = lv_obj_create(card);
        lv_obj_remove_style_all(line);
        lv_obj_set_pos(line, 10, 52);
        lv_obj_set_size(line, w - 20, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x8FC5FF), 0);
        lv_obj_set_style_bg_opa(line, 180, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        make_label(card, 0, 62, w, 24, text, &ui_font_Inter18,
                   COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        return card;
    }

    lv_obj_t *make_screen()
    {
        lv_obj_t *screen = lv_obj_create(NULL);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
        lv_obj_set_style_bg_opa(screen, 255, 0);
        return screen;
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

    void formatDate(const Clock::DateTime &dt, char *out, size_t size)
    {
        snprintf(out, size, "%04d / %02d / %02d", dt.year, dt.month, dt.day);
    }

    void formatTimeShort(const Clock::DateTime &dt, char *out, size_t size)
    {
        snprintf(out, size, "%02d:%02d", dt.hour, dt.minute);
    }

    int letterOfCity(int index)
    {
        const char *name = Location::at(index).name;
        char c = (name && name[0]) ? name[0] : 'A';
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 'A' || c > 'Z') c = 'A';
        return c - 'A';
    }

    bool citiesDirty()
    {
        return draftCity != originalCity;
    }

    bool dateTimeDirty()
    {
        return draftDateTime.year != originalDateTime.year ||
               draftDateTime.month != originalDateTime.month ||
               draftDateTime.day != originalDateTime.day ||
               draftDateTime.hour != originalDateTime.hour ||
               draftDateTime.minute != originalDateTime.minute ||
               draftDateTime.second != originalDateTime.second;
    }

    void refreshOverview()
    {
        Clock::DateTime now = Clock::now();
        char text[48];

        if (overviewTime) {
            formatTimeShort(now, text, sizeof(text));
            lv_label_set_text(overviewTime, text);
        }
        if (overviewDate) {
            formatDate(now, text, sizeof(text));
            lv_label_set_text(overviewDate, text);
        }
        if (overviewCity) {
            lv_label_set_text(overviewCity, Location::city());
        }
        if (overviewOffset) {
            char offset[12];
            formatOffset(Location::offsetForDate(now.year, now.month, now.day),
                         offset, sizeof(offset));
            lv_label_set_text(overviewOffset, offset);
        }
        if (overviewTick) {
            lv_label_set_text(overviewTick, Audio::enabled() ? "Audible" : "Muted");
        }
        if (overviewTickState) {
            lv_label_set_text(overviewTickState, Audio::enabled() ? "ON" : "OFF");
        }
        if (overviewBrightness) {
            snprintf(text, sizeof(text), "%d%%", Brightness::current());
            lv_label_set_text(overviewBrightness, text);
        }

        for (int i = 0; i < 4; i++) {
            if (!overviewCards[i]) continue;
            lv_obj_set_style_bg_color(
                overviewCards[i],
                lv_color_hex(i == overviewFocus ? COLOR_BLUE : COLOR_CARD), 0);
        }
    }

    void refreshReview()
    {
        char date[24];
        char time[16];
        formatDate(draftDateTime, date, sizeof(date));
        formatTimeShort(draftDateTime, time, sizeof(time));
        lv_label_set_text(reviewDateLabel, date);
        lv_label_set_text(reviewTimeLabel, time);
    }

    void refreshTimeScreen()
    {
        char title[48];
        char value[8];
        snprintf(title, sizeof(title), "Set Time (Step 2/2 - %s)",
                 timeStep == 0 ? "Hour" : "Minute");
        lv_label_set_text(timeTitle, title);
        if (timeStep == 0) {
            snprintf(value, sizeof(value), "%02d", draftDateTime.hour);
            lv_label_set_text(timeFieldLabel, "Hour (24-hour)");
        } else {
            snprintf(value, sizeof(value), "%02d", draftDateTime.minute);
            lv_label_set_text(timeFieldLabel, "Minute");
        }
        lv_label_set_text(timeValueLabel, value);
    }

    void refreshTickScreen()
    {
        bool on = Audio::enabled();
        lv_label_set_text(tickStateIcon,
                          on ? "\xEF\x80\xA8" : "\xEF\x9A\xA9");
        lv_label_set_text(tickState, on ? "Audible" : "Muted");
        lv_label_set_text(tickDetail,
                          on ? "1 tick / second | ON" : "No tick sound | OFF");
        lv_label_set_text(tickActionIcon,
                          on ? "\xEF\x9A\xA9" : "\xEF\x80\xA8");
        lv_label_set_text(tickActionLabel, on ? "Mute" : "Unmute");
        lv_label_set_text(tickActionSub,
                          on ? "Stop tick sound" : "Play tick sound");
        lv_obj_set_style_bg_color(tickAction,
                                  lv_color_hex(on ? COLOR_CARD : COLOR_BLUE), 0);
        lv_obj_set_style_border_color(tickAction,
                                      lv_color_hex(on ? COLOR_PINK : COLOR_BLUE),
                                      0);
        lv_obj_set_style_border_width(tickAction, on ? 2 : 0, 0);
    }

    void refreshBrightnessScreen()
    {
        char text[8];
        int value = Brightness::current();
        snprintf(text, sizeof(text), "%d%%", value);
        lv_label_set_text(brightnessValue, text);
        if (brightnessFill) {
            int trackW = 328;
            int fillW = trackW * value / 100;
            if (fillW < 4) fillW = 4;
            lv_obj_set_width(brightnessFill, fillW);
        }
    }

    void loadPage(lv_obj_t *screen, lv_scr_load_anim_t anim)
    {
        lv_scr_load_anim(screen, anim, 150, 0, false);
    }

    void showOverview()
    {
        currentPage = PAGE_OVERVIEW;
        refreshOverview();
        loadPage(overviewScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }

    void syncDateRollers()
    {
        dateSyncing = true;
        int maxDay = Clock::daysInMonth(draftDateTime.year, draftDateTime.month);
        if (draftDateTime.day > maxDay) draftDateTime.day = maxDay;

        char options[512];
        size_t used = 0;
        options[0] = '\0';
        for (int y = 2020; y <= 2069; y++) {
            int n = snprintf(options + used, sizeof(options) - used,
                             "%s%04d", y == 2020 ? "" : "\n", y);
            if (n < 0 || (size_t)n >= sizeof(options) - used) break;
            used += (size_t)n;
        }
        lv_roller_set_options(dateRoller[0], options, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(dateRoller[0],
                               (uint16_t)(draftDateTime.year - 2020), LV_ANIM_OFF);

        used = 0;
        options[0] = '\0';
        for (int m = 1; m <= 12; m++) {
            int n = snprintf(options + used, sizeof(options) - used,
                             "%s%02d", m == 1 ? "" : "\n", m);
            if (n < 0 || (size_t)n >= sizeof(options) - used) break;
            used += (size_t)n;
        }
        lv_roller_set_options(dateRoller[1], options, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(dateRoller[1],
                               (uint16_t)(draftDateTime.month - 1), LV_ANIM_OFF);

        used = 0;
        options[0] = '\0';
        for (int d = 1; d <= maxDay; d++) {
            int n = snprintf(options + used, sizeof(options) - used,
                             "%s%02d", d == 1 ? "" : "\n", d);
            if (n < 0 || (size_t)n >= sizeof(options) - used) break;
            used += (size_t)n;
        }
        lv_roller_set_options(dateRoller[2], options, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(dateRoller[2],
                               (uint16_t)(draftDateTime.day - 1), LV_ANIM_OFF);
        dateSyncing = false;
    }

    void onDateRollerChanged(lv_event_t *event)
    {
        if (dateSyncing) return;

        draftDateTime.year = 2020 + (int)lv_roller_get_selected(dateRoller[0]);
        draftDateTime.month = 1 + (int)lv_roller_get_selected(dateRoller[1]);
        int maxDay = Clock::daysInMonth(draftDateTime.year, draftDateTime.month);
        int selectedDay = 1 + (int)lv_roller_get_selected(dateRoller[2]);
        if (selectedDay > maxDay) selectedDay = maxDay;

        if ((int)lv_roller_get_option_cnt(dateRoller[2]) != maxDay) {
            dateSyncing = true;
            char options[256];
            size_t used = 0;
            options[0] = '\0';
            for (int d = 1; d <= maxDay; d++) {
                int n = snprintf(options + used, sizeof(options) - used,
                                 "%s%02d", d == 1 ? "" : "\n", d);
                if (n < 0 || (size_t)n >= sizeof(options) - used) break;
                used += (size_t)n;
            }
            lv_roller_set_options(dateRoller[2], options,
                                  LV_ROLLER_MODE_NORMAL);
            lv_roller_set_selected(dateRoller[2],
                                   (uint16_t)(selectedDay - 1), LV_ANIM_OFF);
            dateSyncing = false;
        }
        draftDateTime.day = selectedDay;
    }

    void showDateScreen(bool reset)
    {
        currentPage = PAGE_DATE;
        if (reset) {
            draftDateTime = Clock::now();
            originalDateTime = draftDateTime;
            overviewFocus = 0;
        }
        syncDateRollers();
        loadPage(dateScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showTimeScreen(bool hourStep)
    {
        currentPage = PAGE_TIME;
        timeStep = hourStep ? 0 : 1;
        refreshTimeScreen();
        loadPage(timeScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showReview()
    {
        currentPage = PAGE_REVIEW;
        refreshReview();
        loadPage(reviewScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    bool cityMatchesLetter(int index)
    {
        return letterOfCity(index) == locationLetterFilter;
    }

    void rebuildLocationCities()
    {
        locationCityCount = 0;
        for (int i = 0; i < Location::count() && i < 256; i++) {
            if (cityMatchesLetter(i)) {
                locationCityIndices[locationCityCount++] = i;
            }
        }
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

        locationCityOptions[0] = '\0';
        if (locationCityCount == 0) {
            snprintf(locationCityOptions, sizeof(locationCityOptions),
                     "No cities");
        } else {
            size_t used = 0;
            for (int i = 0; i < locationCityCount; i++) {
                int n = snprintf(
                    locationCityOptions + used,
                    sizeof(locationCityOptions) - used,
                    "%s%s", i ? "\n" : "",
                    Location::at(locationCityIndices[i]).name);
                if (n < 0 ||
                    (size_t)n >= sizeof(locationCityOptions) - used) {
                    locationCityCount = i;
                    break;
                }
                used += (size_t)n;
            }
        }
        lv_roller_set_options(locationCityRoller, locationCityOptions,
                              LV_ROLLER_MODE_NORMAL);
    }

    int cityRollerIndexFor(int cityIndex)
    {
        for (int i = 0; i < locationCityCount; i++) {
            if (locationCityIndices[i] == cityIndex) return i;
        }
        return -1;
    }

    void refreshLocationOffset()
    {
        Clock::DateTime now = Clock::now();
        char offset[12];
        formatOffset(Location::offsetForIndex(draftCity, now.year,
                                              now.month, now.day),
                     offset, sizeof(offset));
        lv_label_set_text(locationOffsetLabel, offset);
    }

    void syncLocationRollers()
    {
        locationSyncing = true;
        lv_roller_set_selected(locationLetterRoller,
                               (uint16_t)locationLetterFilter, LV_ANIM_OFF);
        rebuildLocationCities();

        int target = cityRollerIndexFor(draftCity);
        if (target < 0 && locationCityCount > 0) {
            target = 0;
            draftCity = locationCityIndices[0];
            lastCityForLetter[locationLetterFilter] = draftCity;
        }
        if (locationCityCount > 0) {
            lv_roller_set_selected(locationCityRoller, (uint16_t)target,
                                   LV_ANIM_OFF);
        }
        bool enabled = locationCityCount > 0;
        lv_obj_set_style_opa(locationApply, enabled ? 255 : 100, 0);
        locationSyncing = false;
        refreshLocationOffset();
    }

    void onLocationLetterChanged(lv_event_t *event)
    {
        if (locationSyncing) return;

        locationLetterFilter =
            (int)lv_roller_get_selected(lv_event_get_target(event));
        int preferred = lastCityForLetter[locationLetterFilter];

        locationSyncing = true;
        rebuildLocationCities();
        int target = 0;
        if (preferred >= 0) {
            int found = cityRollerIndexFor(preferred);
            if (found >= 0) target = found;
        }
        if (locationCityCount > 0) {
            draftCity = locationCityIndices[target];
            lastCityForLetter[locationLetterFilter] = draftCity;
            lv_roller_set_selected(locationCityRoller, (uint16_t)target,
                                   LV_ANIM_OFF);
        }
        bool enabled = locationCityCount > 0;
        lv_obj_set_style_opa(locationApply, enabled ? 255 : 100, 0);
        locationSyncing = false;
        refreshLocationOffset();
    }

    void onLocationCityChanged(lv_event_t *event)
    {
        if (locationSyncing) return;

        int selected = (int)lv_roller_get_selected(lv_event_get_target(event));
        if (selected >= 0 && selected < locationCityCount) {
            draftCity = locationCityIndices[selected];
            lastCityForLetter[locationLetterFilter] = draftCity;
            refreshLocationOffset();
        }
    }

    void showLocationScreen(bool reset)
    {
        currentPage = PAGE_LOCATION;
        if (reset) {
            draftCity = Location::index();
            originalCity = draftCity;
            locationLetterFilter = letterOfCity(draftCity);
            for (int i = 0; i < 26; i++) lastCityForLetter[i] = -1;
            lastCityForLetter[locationLetterFilter] = draftCity;
            overviewFocus = 1;
        }
        syncLocationRollers();
        loadPage(locationScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showTickScreen()
    {
        currentPage = PAGE_TICK;
        overviewFocus = 2;
        refreshTickScreen();
        loadPage(tickScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showBrightnessScreen()
    {
        currentPage = PAGE_BRIGHTNESS;
        overviewFocus = 3;
        refreshBrightnessScreen();
        loadPage(brightnessScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void configureConfirm(ConfirmMode mode, const char *title,
                          const char *message, const char *sub,
                          const char *action)
    {
        confirmMode = mode;
        lv_label_set_text(confirmIcon,
                          mode == CONFIRM_APPLY ? "\xEF\x81\xB1" : "\xEF\x83\xA2");
        lv_label_set_text(confirmTitle, title);
        lv_label_set_text(confirmMessage, message);
        lv_label_set_text(confirmSub, sub);
        lv_label_set_text(confirmActionLabel, action);
    }

    void showConfirmApply(PendingAction action)
    {
        pendingAction = action;
        currentPage = PAGE_CONFIRM;
        configureConfirm(CONFIRM_APPLY, "Apply Changes",
                         "Apply all changes?",
                         "Your new settings will be saved.", "Apply");
        loadPage(confirmScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showConfirmDiscard(Page source, Page returnPage)
    {
        discardSourcePage = source;
        confirmReturnPage = returnPage;
        currentPage = PAGE_CONFIRM;
        configureConfirm(CONFIRM_DISCARD, "Discard Changes",
                         "Discard changes?",
                         "Your changes will not be saved.", "Discard");
        loadPage(confirmScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    bool applyLocation(int index)
    {
        if (index < 0 || index >= Location::count()) return false;
        if (index == Location::index()) return true;

        Clock::DateTime now = Clock::now();
        int oldOffset = Location::offsetForDate(now.year, now.month, now.day);
        time_t utc = Clock::localEpoch() - (time_t)oldOffset;

        if (!Location::setIndex(index)) return false;
        int newOffset = Location::offsetForUtc(utc);
        if (!Clock::setFromUtc(utc, newOffset)) return false;

        SettingsStore::save();
        Astronomy::update(true);
        return true;
    }

    void confirmApply()
    {
        if (saving) return;
        saving = true;

        bool ok = false;
        if (pendingAction == ACTION_LOCATION) {
            ok = applyLocation(draftCity);
        } else {
            ok = Clock::setDateTime(draftDateTime.year, draftDateTime.month,
                                    draftDateTime.day, draftDateTime.hour,
                                    draftDateTime.minute,
                                    draftDateTime.second);
            if (ok) {
                SettingsStore::save();
                Astronomy::update(true);
            }
        }
        saving = false;

        if (ok) {
            showOverview();
        } else {
            lv_label_set_text(confirmMessage, "Save failed");
            lv_label_set_text(confirmSub, "Try again or cancel.");
        }
    }

    void confirmCancel()
    {
        if (confirmMode == CONFIRM_APPLY) {
            if (pendingAction == ACTION_LOCATION) showLocationScreen(false);
            else showReview();
        } else {
            if (discardSourcePage == PAGE_DATE) showDateScreen(false);
            else showLocationScreen(false);
        }
    }

    void confirmDiscard()
    {
        if (confirmMode == CONFIRM_DISCARD) {
            draftDateTime = originalDateTime;
            draftCity = originalCity;
        }
        showOverview();
    }

    void createOverview()
    {
        overviewScreen = make_screen();
        make_label(overviewScreen, 12, 6, 28, 28, "\xEF\x80\x93",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(overviewScreen, 48, 6, 240, 32, "Settings",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        const int cardX[4] = {8, 134, 260, 386};
        for (int i = 0; i < 4; i++) {
            overviewCards[i] = make_card(overviewScreen, cardX[i], 54, 120,
                                         106, true);
        }

        lv_obj_add_event_cb(overviewCards[0], [](lv_event_t *) {
            overviewFocus = 0;
            showDateScreen(true);
        }, LV_EVENT_CLICKED, NULL);
        make_label(overviewCards[0], 0, 8, 120, 28, "\xEF\x81\xB3",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(overviewCards[0], 0, 38, 120, 16, "Date & Time",
                   &ui_font_Inter15M, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *dtLine = lv_obj_create(overviewCards[0]);
        lv_obj_remove_style_all(dtLine);
        lv_obj_set_pos(dtLine, 12, 54);
        lv_obj_set_size(dtLine, 96, 1);
        lv_obj_set_style_bg_color(dtLine, lv_color_hex(0x8FC5FF), 0);
        lv_obj_set_style_bg_opa(dtLine, 160, 0);
        lv_obj_clear_flag(dtLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        overviewDate = make_label(overviewCards[0], 0, 56, 120, 14, "",
                                  &ui_font_Inter15R, COLOR_TEXT,
                                  LV_TEXT_ALIGN_CENTER);
        overviewTime = make_label(overviewCards[0], 0, 70, 120, 36, "",
                                  &ui_font_Inter28, COLOR_TEXT,
                                  LV_TEXT_ALIGN_CENTER);

        lv_obj_add_event_cb(overviewCards[1], [](lv_event_t *) {
            overviewFocus = 1;
            showLocationScreen(true);
        }, LV_EVENT_CLICKED, NULL);
        make_label(overviewCards[1], 0, 8, 120, 28, "\xEF\x81\x81",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(overviewCards[1], 0, 38, 120, 16, "Location",
                   &ui_font_Inter15M, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *locLine = lv_obj_create(overviewCards[1]);
        lv_obj_remove_style_all(locLine);
        lv_obj_set_pos(locLine, 12, 58);
        lv_obj_set_size(locLine, 96, 1);
        lv_obj_set_style_bg_color(locLine, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(locLine, 255, 0);
        lv_obj_clear_flag(locLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        overviewCity = make_label(overviewCards[1], 0, 64, 120, 22, "",
                                  &ui_font_Inter18, COLOR_TEXT,
                                  LV_TEXT_ALIGN_CENTER);
        overviewOffset = make_label(overviewCards[1], 0, 88, 120, 14, "",
                                    &ui_font_Inter10M, COLOR_MUTED,
                                    LV_TEXT_ALIGN_CENTER);

        lv_obj_add_event_cb(overviewCards[2], [](lv_event_t *) {
            overviewFocus = 2;
            showTickScreen();
        }, LV_EVENT_CLICKED, NULL);
        make_label(overviewCards[2], 0, 8, 120, 28, "\xEF\x80\xA8",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(overviewCards[2], 0, 38, 120, 16, "Tick Sound",
                   &ui_font_Inter15M, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *tickLine = lv_obj_create(overviewCards[2]);
        lv_obj_remove_style_all(tickLine);
        lv_obj_set_pos(tickLine, 12, 58);
        lv_obj_set_size(tickLine, 96, 1);
        lv_obj_set_style_bg_color(tickLine, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(tickLine, 255, 0);
        lv_obj_clear_flag(tickLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        overviewTick = make_label(overviewCards[2], 0, 64, 120, 22, "",
                                  &ui_font_Inter18, COLOR_TEXT,
                                  LV_TEXT_ALIGN_CENTER);
        overviewTickState = make_label(overviewCards[2], 0, 88, 120, 14, "",
                                       &ui_font_Inter10M, COLOR_MUTED,
                                       LV_TEXT_ALIGN_CENTER);

        lv_obj_add_event_cb(overviewCards[3], [](lv_event_t *) {
            overviewFocus = 3;
            showBrightnessScreen();
        }, LV_EVENT_CLICKED, NULL);
        make_label(overviewCards[3], 0, 8, 120, 28, "\xEF\x86\x85",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(overviewCards[3], 0, 38, 120, 16, "Brightness",
                   &ui_font_Inter15M, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *brLine = lv_obj_create(overviewCards[3]);
        lv_obj_remove_style_all(brLine);
        lv_obj_set_pos(brLine, 12, 58);
        lv_obj_set_size(brLine, 96, 1);
        lv_obj_set_style_bg_color(brLine, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(brLine, 255, 0);
        lv_obj_clear_flag(brLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        overviewBrightness = make_label(overviewCards[3], 0, 62, 120, 40, "",
                                         &ui_font_Inter28, COLOR_TEXT,
                                         LV_TEXT_ALIGN_CENTER);

        lv_obj_t *backCard = make_card(overviewScreen, 512, 54, 116, 106, true);
        lv_obj_add_event_cb(backCard, [](lv_event_t *) {
            Settings::hide(0);
        }, LV_EVENT_CLICKED, NULL);
        make_label(backCard, 0, 14, 116, 28, "\xEF\x83\xA2",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(backCard, 0, 56, 116, 18, "Back to",
                   &ui_font_Inter15M, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(backCard, 0, 76, 116, 20, "Clock",
                   &ui_font_Inter18, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
    }

    void style_roller(lv_obj_t *roller)
    {
        lv_obj_set_style_bg_opa(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, &ui_font_Inter18, LV_PART_MAIN);
        lv_obj_set_style_text_color(roller, lv_color_hex(0x7F8A99),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(roller, lv_color_hex(COLOR_BLUE),
                                  LV_PART_SELECTED);
        lv_obj_set_style_bg_opa(roller, 255, LV_PART_SELECTED);
        lv_obj_set_style_text_color(roller, lv_color_hex(COLOR_TEXT),
                                    LV_PART_SELECTED);
        lv_obj_set_style_radius(roller, 10, LV_PART_SELECTED);
        lv_obj_clear_flag(roller, LV_OBJ_FLAG_SCROLL_CHAIN);
    }

    lv_obj_t *make_roller_card(lv_obj_t *parent, int x, int y, int w, int h,
                               const char *label)
    {
        lv_obj_t *card = make_card(parent, x, y, w, h);
        make_label(card, 0, 8, w, 14, label, &ui_font_Inter15M, COLOR_MUTED,
                   LV_TEXT_ALIGN_CENTER);
        lv_obj_t *line = lv_obj_create(card);
        lv_obj_remove_style_all(line);
        lv_obj_set_pos(line, 8, 26);
        lv_obj_set_size(line, w - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(line, 255, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *roller = lv_roller_create(card);
        style_roller(roller);
        lv_obj_set_pos(roller, 8, 32);
        lv_obj_set_width(roller, w - 16);
        lv_roller_set_visible_row_count(roller, 3);
        return roller;
    }

    void createDateScreen()
    {
        dateScreen = make_screen();
        make_label(dateScreen, 12, 4, 28, 28, "\xEF\x81\xB3",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(dateScreen, 50, 6, 400, 30, "Set Date (Step 1/2)",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        make_back_card(dateScreen, 12, 57, 100, 100, [](lv_event_t *) {
            if (dateTimeDirty()) showConfirmDiscard(PAGE_DATE, PAGE_OVERVIEW);
            else showOverview();
        });
        dateRoller[0] = make_roller_card(dateScreen, 124, 57, 140, 100, "Year");
        dateRoller[1] = make_roller_card(dateScreen, 274, 57, 120, 100, "Month");
        dateRoller[2] = make_roller_card(dateScreen, 404, 57, 120, 100, "Day");
        lv_obj_add_event_cb(dateRoller[0], onDateRollerChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(dateRoller[1], onDateRollerChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(dateRoller[2], onDateRollerChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);
        make_primary_card(dateScreen, 534, 57, 94, 100, "\xEF\x81\x94",
                          "Next", [](lv_event_t *) { showTimeScreen(true); });
    }

    void createTimeScreen()
    {
        timeScreen = make_screen();
        make_label(timeScreen, 12, 4, 28, 28, "\xEF\x80\x93",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        timeTitle = make_label(timeScreen, 50, 6, 560, 30, "",
                               &ui_font_Inter28, COLOR_TEXT,
                               LV_TEXT_ALIGN_LEFT);

        make_back_card(timeScreen, 12, 57, 100, 100, [](lv_event_t *) {
            if (timeStep == 0) showDateScreen(false);
            else showTimeScreen(true);
        });

        lv_obj_t *minus = make_card(timeScreen, 120, 57, 100, 100, true);
        lv_obj_add_event_cb(minus, [](lv_event_t *) {
            if (timeStep == 0) draftDateTime.hour = (draftDateTime.hour + 23) % 24;
            else draftDateTime.minute = (draftDateTime.minute + 59) % 60;
            refreshTimeScreen();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *minusLine = lv_obj_create(minus);
        lv_obj_remove_style_all(minusLine);
        lv_obj_set_size(minusLine, 44, 5);
        lv_obj_align(minusLine, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(minusLine, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_bg_opa(minusLine, 255, 0);
        lv_obj_set_style_radius(minusLine, 2, 0);
        lv_obj_clear_flag(minusLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *valueCard = make_card(timeScreen, 228, 57, 200, 100);
        make_label(valueCard, 0, 8, 200, 28, "\xEF\x80\x97",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        timeValueLabel = make_label(valueCard, 0, 30, 200, 50, "",
                                    &ui_font_Inter42, COLOR_TEXT,
                                    LV_TEXT_ALIGN_CENTER);
        timeFieldLabel = make_label(valueCard, 0, 82, 200, 16, "",
                                    &ui_font_Inter15M, COLOR_MUTED,
                                    LV_TEXT_ALIGN_CENTER);

        lv_obj_t *plus = make_card(timeScreen, 436, 57, 100, 100, true);
        lv_obj_add_event_cb(plus, [](lv_event_t *) {
            if (timeStep == 0) draftDateTime.hour = (draftDateTime.hour + 1) % 24;
            else draftDateTime.minute = (draftDateTime.minute + 1) % 60;
            refreshTimeScreen();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *plusLabel = lv_label_create(plus);
        lv_obj_set_width(plusLabel, 100);
        lv_label_set_text(plusLabel, "+");
        lv_obj_set_style_text_font(plusLabel, &ui_font_Inter42, 0);
        lv_obj_set_style_text_color(plusLabel, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_text_align(plusLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(plusLabel);

        make_primary_card(timeScreen, 544, 57, 84, 100, "\xEF\x81\x94",
                          "Next", [](lv_event_t *) {
                              if (timeStep == 0) showTimeScreen(false);
                              else showReview();
                          });
    }

    void createReviewScreen()
    {
        reviewScreen = make_screen();
        make_label(reviewScreen, 12, 4, 28, 28, "\xEF\x81\xB3",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(reviewScreen, 50, 6, 500, 30, "Review Date & Time",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        make_back_card(reviewScreen, 12, 57, 120, 100, [](lv_event_t *) {
            showTimeScreen(false);
        });
        lv_obj_t *summary = make_card(reviewScreen, 144, 57, 370, 100);
        make_label(summary, 0, 8, 370, 28, "\xEF\x80\x97",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *line = lv_obj_create(summary);
        lv_obj_remove_style_all(line);
        lv_obj_set_pos(line, 24, 36);
        lv_obj_set_size(line, 322, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(line, 255, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        reviewDateLabel = make_label(summary, 0, 44, 370, 18, "",
                                     &ui_font_Inter18, COLOR_TEXT,
                                     LV_TEXT_ALIGN_CENTER);
        reviewTimeLabel = make_label(summary, 0, 60, 370, 38, "",
                                     &ui_font_Inter28, COLOR_TEXT,
                                     LV_TEXT_ALIGN_CENTER);

        make_primary_card(reviewScreen, 526, 57, 102, 100, "\xEF\x80\x8C",
                          "Apply", [](lv_event_t *) {
                              showConfirmApply(ACTION_DATETIME);
                          });
    }

    void createLocationScreen()
    {
        locationScreen = make_screen();
        make_label(locationScreen, 12, 4, 28, 28, "\xEF\x81\x81",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(locationScreen, 50, 6, 500, 30, "Select Location",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        make_back_card(locationScreen, 12, 57, 100, 100, [](lv_event_t *) {
            if (citiesDirty()) showConfirmDiscard(PAGE_LOCATION, PAGE_OVERVIEW);
            else showOverview();
        });

        lv_obj_t *letterCard = make_card(locationScreen, 120, 57, 120, 100);
        make_label(letterCard, 0, 8, 120, 14, "Letter", &ui_font_Inter15M,
                   COLOR_MUTED, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *letterLine = lv_obj_create(letterCard);
        lv_obj_remove_style_all(letterLine);
        lv_obj_set_pos(letterLine, 8, 26);
        lv_obj_set_size(letterLine, 104, 1);
        lv_obj_set_style_bg_color(letterLine, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(letterLine, 255, 0);
        lv_obj_clear_flag(letterLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        locationLetterRoller = lv_roller_create(letterCard);
        style_roller(locationLetterRoller);
        lv_obj_set_pos(locationLetterRoller, 8, 32);
        lv_obj_set_width(locationLetterRoller, 104);
        lv_roller_set_options(locationLetterRoller, locationLetterOptions,
                              LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationLetterRoller, 3);
        lv_obj_add_event_cb(locationLetterRoller, onLocationLetterChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *cityCard = make_card(locationScreen, 260, 57, 258, 100);
        make_label(cityCard, 0, 8, 258, 14, "City", &ui_font_Inter15M,
                   COLOR_MUTED, LV_TEXT_ALIGN_CENTER);
        locationOffsetLabel = make_label(cityCard, 184, 8, 66, 14, "",
                                         &ui_font_Inter10M, COLOR_MUTED,
                                         LV_TEXT_ALIGN_RIGHT);
        lv_obj_t *cityLine = lv_obj_create(cityCard);
        lv_obj_remove_style_all(cityLine);
        lv_obj_set_pos(cityLine, 8, 26);
        lv_obj_set_size(cityLine, 242, 1);
        lv_obj_set_style_bg_color(cityLine, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(cityLine, 255, 0);
        lv_obj_clear_flag(cityLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        locationCityRoller = lv_roller_create(cityCard);
        style_roller(locationCityRoller);
        lv_obj_set_pos(locationCityRoller, 8, 32);
        lv_obj_set_width(locationCityRoller, 242);
        lv_roller_set_options(locationCityRoller, "", LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationCityRoller, 3);
        lv_obj_add_event_cb(locationCityRoller, onLocationCityChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_move_foreground(locationCityRoller);

        locationApply = make_primary_card(locationScreen, 526, 57, 102, 100,
                                          "\xEF\x80\x8C", "Apply",
                                          [](lv_event_t *) {
                                              if (locationCityCount > 0) {
                                                  showConfirmApply(ACTION_LOCATION);
                                              }
                                          });
    }

    void createTickScreen()
    {
        tickScreen = make_screen();
        make_label(tickScreen, 12, 4, 28, 28, "\xEF\x80\xA8",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(tickScreen, 50, 6, 500, 30, "Tick Sound",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        make_back_card(tickScreen, 12, 57, 150, 100, [](lv_event_t *) {
            showOverview();
        });

        lv_obj_t *stateCard = make_card(tickScreen, 172, 57, 220, 100);
        tickStateIcon = make_label(stateCard, 0, 8, 220, 28, "\xEF\x80\xA8",
                                   &ui_font_FA28, COLOR_TEXT,
                                   LV_TEXT_ALIGN_CENTER);
        tickState = make_label(stateCard, 0, 36, 220, 34, "",
                               &ui_font_Inter28, COLOR_TEXT,
                               LV_TEXT_ALIGN_CENTER);
        tickDetail = make_label(stateCard, 0, 70, 220, 18, "",
                                &ui_font_Inter15M, COLOR_MUTED,
                                LV_TEXT_ALIGN_CENTER);

        tickAction = make_card(tickScreen, 402, 57, 226, 100, true);
        lv_obj_set_style_border_width(tickAction, 2, 0);
        lv_obj_add_event_cb(tickAction, [](lv_event_t *) {
            Audio::setEnabled(!Audio::enabled(), true);
            SettingsStore::save();
            refreshTickScreen();
        }, LV_EVENT_CLICKED, NULL);
        tickActionIcon = make_label(tickAction, 0, 8, 226, 28, "\xEF\x80\xA8",
                                    &ui_font_FA28, COLOR_TEXT,
                                    LV_TEXT_ALIGN_CENTER);
        tickActionLabel = make_label(tickAction, 0, 36, 226, 34, "",
                                     &ui_font_Inter28, COLOR_TEXT,
                                     LV_TEXT_ALIGN_CENTER);
        tickActionSub = make_label(tickAction, 0, 70, 226, 18, "",
                                   &ui_font_Inter15M, COLOR_TEXT,
                                   LV_TEXT_ALIGN_CENTER);
    }

    void createBrightnessScreen()
    {
        brightnessScreen = make_screen();
        make_label(brightnessScreen, 12, 4, 28, 28, "\xEF\x86\x85",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        make_label(brightnessScreen, 50, 6, 500, 30, "Brightness",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);

        make_back_card(brightnessScreen, 12, 57, 120, 100, [](lv_event_t *) {
            showOverview();
        });

        lv_obj_t *control = make_card(brightnessScreen, 140, 57, 488, 100);
        make_label(control, 0, 5, 488, 28, "\xEF\x86\x85",
                   &ui_font_FA28, COLOR_TEXT, LV_TEXT_ALIGN_CENTER);

        lv_obj_t *minus = make_card(control, 12, 28, 56, 44, true);
        lv_obj_add_event_cb(minus, [](lv_event_t *) {
            int v = Brightness::current() - 5;
            if (v < 40) v = 40;
            Brightness::set(v);
            refreshBrightnessScreen();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *minusLine = lv_obj_create(minus);
        lv_obj_remove_style_all(minusLine);
        lv_obj_set_size(minusLine, 28, 4);
        lv_obj_align(minusLine, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(minusLine, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_bg_opa(minusLine, 255, 0);
        lv_obj_set_style_radius(minusLine, 2, 0);
        lv_obj_clear_flag(minusLine, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *plus = make_card(control, 420, 28, 56, 44, true);
        lv_obj_add_event_cb(plus, [](lv_event_t *) {
            int v = Brightness::current() + 5;
            if (v > 100) v = 100;
            Brightness::set(v);
            refreshBrightnessScreen();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *plusLabel = lv_label_create(plus);
        lv_obj_set_width(plusLabel, 56);
        lv_label_set_text(plusLabel, "+");
        lv_obj_set_style_text_font(plusLabel, &ui_font_Inter28, 0);
        lv_obj_set_style_text_color(plusLabel, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_text_align(plusLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(plusLabel);

        lv_obj_t *track = make_card(control, 80, 42, 328, 10);
        lv_obj_set_style_bg_color(track, lv_color_hex(0x2B333D), 0);
        lv_obj_set_style_radius(track, 5, 0);
        brightnessFill = lv_obj_create(track);
        lv_obj_remove_style_all(brightnessFill);
        lv_obj_set_pos(brightnessFill, 0, 0);
        lv_obj_set_size(brightnessFill, 10, 10);
        lv_obj_set_style_bg_color(brightnessFill, lv_color_hex(COLOR_BLUE), 0);
        lv_obj_set_style_bg_opa(brightnessFill, 255, 0);
        lv_obj_set_style_radius(brightnessFill, 5, 0);
        lv_obj_clear_flag(brightnessFill, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        brightnessValue = make_label(control, 80, 58, 328, 28, "",
                                      &ui_font_Inter28, COLOR_TEXT,
                                      LV_TEXT_ALIGN_CENTER);
    }

    void createConfirmScreen()
    {
        confirmScreen = make_screen();
        confirmIcon = make_label(confirmScreen, 12, 4, 28, 28, "",
                                 &ui_font_FA28, COLOR_TEXT,
                                 LV_TEXT_ALIGN_CENTER);
        confirmTitle = make_label(confirmScreen, 50, 6, 500, 30, "",
                                  &ui_font_Inter28, COLOR_TEXT,
                                  LV_TEXT_ALIGN_LEFT);

        lv_obj_t *card = make_card(confirmScreen, 12, 57, 616, 100);
        lv_obj_t *circle = lv_obj_create(card);
        lv_obj_remove_style_all(circle);
        lv_obj_set_pos(circle, 24, 20);
        lv_obj_set_size(circle, 60, 60);
        lv_obj_set_style_bg_color(circle, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_bg_opa(circle, 255, 0);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        make_label(circle, 0, 0, 60, 60, "?", &ui_font_Inter42,
                   COLOR_BG, LV_TEXT_ALIGN_CENTER);

        confirmMessage = make_label(card, 100, 16, 260, 28, "",
                                    &ui_font_Inter28, COLOR_TEXT,
                                    LV_TEXT_ALIGN_LEFT);
        confirmSub = make_label(card, 100, 48, 260, 18, "",
                                &ui_font_Inter15M, COLOR_MUTED,
                                LV_TEXT_ALIGN_LEFT);

        make_action_card(card, 380, 20, 100, 60, "Cancel",
                         &ui_font_Inter18, 0x232B34, 0x4A5563, COLOR_TEXT,
                         [](lv_event_t *) { confirmCancel(); });
        lv_obj_t *action = make_action_card(card, 490, 20, 126, 60, "",
                                            &ui_font_Inter18, COLOR_BLUE,
                                            0, COLOR_TEXT,
                                            [](lv_event_t *) {
                                                if (confirmMode == CONFIRM_APPLY) {
                                                    confirmApply();
                                                } else {
                                                    confirmDiscard();
                                                }
                                            });
        confirmActionLabel = lv_obj_get_child(action, 0);
    }
}

namespace Settings
{
    void begin()
    {
        if (overviewScreen) return;

        createOverview();
        createDateScreen();
        createTimeScreen();
        createReviewScreen();
        createLocationScreen();
        createTickScreen();
        createBrightnessScreen();
        createConfirmScreen();

        lv_obj_add_flag(ui_Screen1_Panel_border, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_Screen1_Panel_border, [](lv_event_t *) {
            Settings::show();
        }, LV_EVENT_CLICKED, NULL);

        refreshOverview();
    }

    void update()
    {
        if (!openFlag || currentPage != PAGE_OVERVIEW) return;

        unsigned long now = millis();
        if (now - lastRefreshMs >= 250) {
            refreshOverview();
            lastRefreshMs = now;
        }
    }

    void show(int swipeDx)
    {
        if (!overviewScreen || openFlag) return;
        openFlag = true;
        currentPage = PAGE_OVERVIEW;
        refreshOverview();
        lv_scr_load_anim(overviewScreen,
                         swipeDx < 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT :
                         swipeDx > 0 ? LV_SCR_LOAD_ANIM_MOVE_RIGHT :
                         LV_SCR_LOAD_ANIM_MOVE_LEFT,
                         150, 0, false);
    }

    void hide(int swipeDx)
    {
        if (!openFlag) return;
        openFlag = false;
        currentPage = PAGE_OVERVIEW;
        lv_scr_load_anim(ui_Screen1,
                         swipeDx < 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT :
                         swipeDx > 0 ? LV_SCR_LOAD_ANIM_MOVE_RIGHT :
                         LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                         150, 0, false);
        MoonIntro::play();
    }

    bool isOpen()
    {
        return openFlag;
    }

    bool swipeBlocked()
    {
        return openFlag && currentPage != PAGE_OVERVIEW;
    }

    bool isPreviewActive()
    {
        return false;
    }

    void showLocationPicker()
    {
        if (openFlag) showLocationScreen(true);
    }

    void hideLocationPicker(int)
    {
        if (currentPage == PAGE_LOCATION) showOverview();
    }

    bool isLocationPickerOpen()
    {
        return currentPage == PAGE_LOCATION;
    }

    void showDateTimePicker()
    {
        if (openFlag) showDateScreen(true);
    }

    void hideDateTimePicker(bool)
    {
        if (currentPage == PAGE_DATE || currentPage == PAGE_TIME ||
            currentPage == PAGE_REVIEW) {
            showOverview();
        }
    }

    bool isDateTimePickerOpen()
    {
        return currentPage == PAGE_DATE || currentPage == PAGE_TIME ||
               currentPage == PAGE_REVIEW;
    }

    void adjustField(int field, int delta)
    {
        Clock::DateTime dt = Clock::now();

        switch (field) {
        case 0:
            dt.year += delta;
            if (dt.year < 2020) dt.year = 2020;
            if (dt.year > 2069) dt.year = 2069;
            break;
        case 1:
            dt.month += delta;
            if (dt.month < 1) dt.month = 12;
            if (dt.month > 12) dt.month = 1;
            break;
        case 2:
            dt.day += delta;
            if (dt.day < 1) dt.day = 1;
            if (dt.day > Clock::daysInMonth(dt.year, dt.month)) {
                dt.day = Clock::daysInMonth(dt.year, dt.month);
            }
            break;
        case 3:
            dt.hour = (dt.hour + delta + 24) % 24;
            break;
        case 4:
            dt.minute = (dt.minute + delta + 60) % 60;
            break;
        case 5:
            dt.second = (dt.second + delta + 60) % 60;
            break;
        default:
            return;
        }

        if (dt.day > Clock::daysInMonth(dt.year, dt.month)) {
            dt.day = Clock::daysInMonth(dt.year, dt.month);
        }
        if (Clock::setDateTime(dt.year, dt.month, dt.day, dt.hour, dt.minute,
                               dt.second)) {
            SettingsStore::save();
            Astronomy::update(true);
            if (openFlag) refreshOverview();
        }
    }

    void selectLocation(int index)
    {
        if (applyLocation(index) && openFlag) refreshOverview();
    }

    void debugOpen(int page)
    {
        if (!openFlag) return;

        switch (page) {
        case 0:
            showOverview();
            break;
        case 1:
            showDateScreen(true);
            break;
        case 2:
            showTimeScreen(true);
            break;
        case 3:
            showTimeScreen(false);
            break;
        case 4:
            draftDateTime = Clock::now();
            originalDateTime = draftDateTime;
            showReview();
            break;
        case 5:
            showLocationScreen(true);
            break;
        case 6:
            showTickScreen();
            break;
        case 7:
            showBrightnessScreen();
            break;
        case 8:
            showConfirmApply(ACTION_DATETIME);
            break;
        case 9:
            showConfirmDiscard(PAGE_DATE, PAGE_OVERVIEW);
            break;
        default:
            break;
        }
    }
}
