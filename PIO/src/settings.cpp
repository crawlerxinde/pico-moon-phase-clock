#include "settings.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astronomy.h"
#include "audio_tick.h"
#include "battery.h"
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
        PAGE_CHOICE,
        PAGE_EDITOR,
        PAGE_REVIEW,
        PAGE_LOCATION,
        PAGE_TICK,
        PAGE_DISCARD
    };

    enum ChoiceMode
    {
        CHOICE_DATETIME,
        CHOICE_DISPLAY
    };

    enum EditorMode
    {
        EDIT_TIME,
        EDIT_DATE,
        EDIT_BRIGHTNESS
    };

    enum ReviewMode
    {
        REVIEW_TIME,
        REVIEW_DATE,
        REVIEW_LOCATION
    };

    enum EditStep
    {
        STEP_HOUR,
        STEP_MINUTE,
        STEP_YEAR,
        STEP_MONTH,
        STEP_DAY,
        STEP_BRIGHTNESS
    };

    constexpr uint32_t COLOR_BG          = 0x0E1116;
    constexpr uint32_t COLOR_CARD        = 0x1A2027;
    constexpr uint32_t COLOR_CARD_PRESS  = 0x252D37;
    constexpr uint32_t COLOR_TEXT        = 0xFFFFFF;
    constexpr uint32_t COLOR_MUTED       = 0x8A94A3;
    constexpr uint32_t COLOR_ACCENT      = 0x5A8DEE;
    constexpr uint32_t COLOR_ACTION      = 0x3D6FA8;
    constexpr uint32_t COLOR_ACTION_EDGE = 0x6FA8FF;

    bool openFlag = false;
    Page currentPage = PAGE_OVERVIEW;
    ChoiceMode choiceMode = CHOICE_DATETIME;
    EditorMode editorMode = EDIT_TIME;
    ReviewMode reviewMode = REVIEW_TIME;
    EditStep editStep = STEP_HOUR;

    Clock::DateTime draftDateTime = {2026, 1, 1, 0, 0, 0, 0};
    Clock::DateTime originalDateTime = {2026, 1, 1, 0, 0, 0, 0};
    int draftBrightness = 100;
    int originalBrightness = 100;
    bool draftTick = true;
    bool originalTick = true;
    bool previewActive = false;
    bool saving = false;

    Page discardSource = PAGE_OVERVIEW;
    Page discardReturn = PAGE_OVERVIEW;

    unsigned long lastRefreshMs = 0;

    lv_obj_t *overviewScreen = nullptr;
    lv_obj_t *choiceScreen = nullptr;
    lv_obj_t *editorScreen = nullptr;
    lv_obj_t *reviewScreen = nullptr;
    lv_obj_t *locationScreen = nullptr;
    lv_obj_t *tickScreen = nullptr;
    lv_obj_t *discardScreen = nullptr;

    /* Overview */
    lv_obj_t *overviewBatLabel = nullptr;
    lv_obj_t *overviewTime = nullptr;
    lv_obj_t *overviewSeconds = nullptr;
    lv_obj_t *overviewDate = nullptr;
    lv_obj_t *overviewCity = nullptr;
    lv_obj_t *overviewOffset = nullptr;
    lv_obj_t *overviewBrightness = nullptr;

    /* Choice */
    lv_obj_t *choiceTitle = nullptr;
    lv_obj_t *choiceModeLabel = nullptr;
    lv_obj_t *choiceCard1Label = nullptr;
    lv_obj_t *choiceCard1Value = nullptr;
    lv_obj_t *choiceCard1Sub = nullptr;
    lv_obj_t *choiceCard2Label = nullptr;
    lv_obj_t *choiceCard2Value = nullptr;
    lv_obj_t *choiceCard2Sub = nullptr;

    /* Editor */
    lv_obj_t *editorTitle = nullptr;
    lv_obj_t *editorStepLabel = nullptr;
    lv_obj_t *editorFieldLabel = nullptr;
    lv_obj_t *editorValueLabel = nullptr;
    lv_obj_t *editorSubLabel = nullptr;
    lv_obj_t *editorActionLabel = nullptr;

    /* Review */
    lv_obj_t *reviewTitle = nullptr;
    lv_obj_t *reviewStatus = nullptr;
    lv_obj_t *reviewLabel = nullptr;
    lv_obj_t *reviewValue = nullptr;
    lv_obj_t *reviewOldNew = nullptr;

    /* Location */
    lv_obj_t *locationTitle = nullptr;
    lv_obj_t *locationLetterRoller = nullptr;
    lv_obj_t *locationCityRoller = nullptr;
    lv_obj_t *locationReview = nullptr;
    lv_obj_t *locationHint = nullptr;
    const char locationLetterOptions[] = "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
    char locationCityOptions[4096] = {0};
    int locationCityIndices[256] = {0};
    int locationCityCount = 0;
    int locationLetterFilter = 0;
    int locationDraftCity = 0;
    int lastCityForLetter[26];
    bool locationSyncing = false;
    bool locationReviewEnabled = false;

    /* Tick sound */
    lv_obj_t *tickTitle = nullptr;
    lv_obj_t *tickStateLabel = nullptr;
    lv_obj_t *tickDetailLabel = nullptr;
    lv_obj_t *tickToggleLabel = nullptr;
    lv_obj_t *tickToggleSub = nullptr;
    lv_obj_t *tickToggleCard = nullptr;

    /* Discard */
    lv_obj_t *discardMessage = nullptr;

    void showOverview();
    void showChoice(ChoiceMode mode);
    void showEditorPage(bool initialize);
    void showReview(ReviewMode mode);
    void showLocationPage(bool initialize);
    void showTickPage(bool initialize);
    void showDiscard(Page source, Page returnPage);

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
                               lv_event_cb_t cb, void *userData = nullptr)
    {
        lv_obj_t *card = make_card(parent, x, y, w, h, true);
        lv_obj_set_style_bg_color(card, lv_color_hex(bg), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(border), 0);
        lv_obj_set_style_border_width(card, border ? 2 : 0, 0);
        lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, userData);
        lv_obj_t *label = lv_label_create(card);
        lv_obj_set_width(label, w);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        return card;
    }

    lv_obj_t *make_back_button(lv_obj_t *parent, lv_event_cb_t cb)
    {
        return make_action_card(parent, 8, 2, 86, 46, "< Back",
                                &ui_font_Inter18, 0x1A2027, 0x2E3945,
                                COLOR_TEXT, cb);
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

    void formatTimeShort(const Clock::DateTime &dt, char *out, size_t size,
                         bool withSeconds = false)
    {
        if (withSeconds) {
            snprintf(out, size, "%02d:%02d:%02d", dt.hour, dt.minute,
                     dt.second);
        } else {
            snprintf(out, size, "%02d:%02d", dt.hour, dt.minute);
        }
    }

    void formatDate(const Clock::DateTime &dt, char *out, size_t size)
    {
        snprintf(out, size, "%04d / %02d / %02d", dt.year, dt.month, dt.day);
    }

    bool sameDraftDateTime()
    {
        return draftDateTime.year == originalDateTime.year &&
               draftDateTime.month == originalDateTime.month &&
               draftDateTime.day == originalDateTime.day &&
               draftDateTime.hour == originalDateTime.hour &&
               draftDateTime.minute == originalDateTime.minute &&
               draftDateTime.second == originalDateTime.second;
    }

    bool editorDirty()
    {
        if (editorMode == EDIT_BRIGHTNESS) {
            return draftBrightness != originalBrightness;
        }
        return !sameDraftDateTime();
    }

    void refreshOverview()
    {
        Clock::DateTime now = Clock::now();
        char text[40];

        Battery::update();
        if (Battery::present()) {
            snprintf(text, sizeof(text), "BAT %d%%  |  %d%%",
                     Battery::percent(), Brightness::current());
        } else {
            snprintf(text, sizeof(text), "BAT USB  |  %d%%",
                     Brightness::current());
        }
        if (overviewBatLabel) lv_label_set_text(overviewBatLabel, text);

        if (overviewTime) {
            formatTimeShort(now, text, sizeof(text));
            lv_label_set_text(overviewTime, text);
        }
        if (overviewSeconds) {
            snprintf(text, sizeof(text), ":%02d", now.second);
            lv_label_set_text(overviewSeconds, text);
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
        if (overviewBrightness) {
            snprintf(text, sizeof(text), "%d%%", Brightness::current());
            lv_label_set_text(overviewBrightness, text);
        }
    }

    void refreshChoice()
    {
        Clock::DateTime now = Clock::now();
        char value[32];
        char sub[40];

        lv_obj_set_style_text_font(
            choiceCard2Value,
            choiceMode == CHOICE_DATETIME ? &ui_font_Inter28 : &ui_font_Inter42,
            0);

        if (choiceMode == CHOICE_DATETIME) {
            lv_label_set_text(choiceTitle, "Date & Time");
            lv_label_set_text(choiceModeLabel, "DATE & TIME");
            lv_label_set_text(choiceCard1Label, "SET TIME");
            formatTimeShort(now, value, sizeof(value));
            lv_label_set_text(choiceCard1Value, value);
            lv_label_set_text(choiceCard1Sub, "Hour / minute");
            lv_label_set_text(choiceCard2Label, "SET DATE");
            formatDate(now, value, sizeof(value));
            lv_label_set_text(choiceCard2Value, value);
            lv_label_set_text(choiceCard2Sub, "Year / month / day");
        } else {
            lv_label_set_text(choiceTitle, "Display");
            lv_label_set_text(choiceModeLabel, "DISPLAY");
            lv_label_set_text(choiceCard1Label, "BRIGHTNESS");
            snprintf(value, sizeof(value), "%d%%", Brightness::current());
            lv_label_set_text(choiceCard1Value, value);
            lv_label_set_text(choiceCard1Sub, "Step: 5%");
            lv_label_set_text(choiceCard2Label, "TICK SOUND");
            lv_label_set_text(choiceCard2Value,
                              Audio::enabled() ? "Audible" : "Muted");
            snprintf(sub, sizeof(sub), "1 tick / second  |  %s",
                     Audio::enabled() ? "ON" : "OFF");
            lv_label_set_text(choiceCard2Sub, sub);
        }
    }

    void refreshEditor()
    {
        char text[48];
        char step[40];

        lv_obj_set_style_text_font(editorValueLabel,
            editorMode == EDIT_DATE ? &ui_font_Inter28 : &ui_font_Inter42, 0);

        switch (editorMode) {
        case EDIT_TIME:
            lv_label_set_text(editorTitle, "Set time");
            formatTimeShort(draftDateTime, text, sizeof(text));
            lv_label_set_text(editorValueLabel, text);
            lv_label_set_text(editorSubLabel, "");
            if (editStep == STEP_HOUR) {
                lv_label_set_text(editorStepLabel, "STEP 1 / 2  |  HOUR");
                lv_label_set_text(editorFieldLabel, "HOUR");
                lv_label_set_text(editorActionLabel, "Next >");
            } else {
                lv_label_set_text(editorStepLabel, "STEP 2 / 2  |  MINUTE");
                lv_label_set_text(editorFieldLabel, "MINUTE");
                lv_label_set_text(editorActionLabel, "Review >");
            }
            break;

        case EDIT_DATE:
            lv_label_set_text(editorTitle, "Set date");
            formatDate(draftDateTime, text, sizeof(text));
            lv_label_set_text(editorValueLabel, text);
            lv_label_set_text(editorSubLabel, "");
            if (editStep == STEP_YEAR) {
                lv_label_set_text(editorStepLabel, "STEP 1 / 3  |  YEAR");
                lv_label_set_text(editorFieldLabel, "YEAR");
                lv_label_set_text(editorActionLabel, "Next >");
            } else if (editStep == STEP_MONTH) {
                lv_label_set_text(editorStepLabel, "STEP 2 / 3  |  MONTH");
                lv_label_set_text(editorFieldLabel, "MONTH");
                lv_label_set_text(editorActionLabel, "Next >");
            } else {
                lv_label_set_text(editorStepLabel, "STEP 3 / 3  |  DAY");
                lv_label_set_text(editorFieldLabel, "DAY");
                lv_label_set_text(editorActionLabel, "Review >");
            }
            break;

        case EDIT_BRIGHTNESS:
            lv_label_set_text(editorTitle, "Brightness");
            snprintf(step, sizeof(step), "PREVIEW  |  %d%%",
                     draftBrightness);
            lv_label_set_text(editorStepLabel, step);
            lv_label_set_text(editorFieldLabel, "BRIGHTNESS");
            snprintf(text, sizeof(text), "%d%%", draftBrightness);
            lv_label_set_text(editorValueLabel, text);
            lv_label_set_text(editorSubLabel, "Step: 5%");
            lv_label_set_text(editorActionLabel, "Save");
            break;
        }
    }

    void refreshReview()
    {
        char value[64];
        char oldNew[128];

        switch (reviewMode) {
        case REVIEW_TIME: {
            Clock::DateTime old = originalDateTime;
            char oldText[24];
            formatTimeShort(old, oldText, sizeof(oldText), true);
            formatTimeShort(draftDateTime, value, sizeof(value), true);
            snprintf(oldNew, sizeof(oldNew), "%s  >  %s",
                     oldText, value);
            lv_label_set_text(reviewTitle, "Confirm changes");
            lv_label_set_text(reviewLabel, "TIME");
            lv_label_set_text(reviewValue, value);
            lv_label_set_text(reviewOldNew, oldNew);
            break;
        }

        case REVIEW_DATE: {
            char oldText[24];
            formatDate(originalDateTime, oldText, sizeof(oldText));
            formatDate(draftDateTime, value, sizeof(value));
            snprintf(oldNew, sizeof(oldNew), "%s  >  %s",
                     oldText, value);
            lv_label_set_text(reviewTitle, "Confirm changes");
            lv_label_set_text(reviewLabel, "DATE");
            lv_label_set_text(reviewValue, value);
            lv_label_set_text(reviewOldNew, oldNew);
            break;
        }

        case REVIEW_LOCATION: {
            Clock::DateTime now = Clock::now();
            char oldOffset[12];
            char newOffset[12];
            formatOffset(Location::offsetForIndex(Location::index(),
                                                 now.year, now.month, now.day),
                         oldOffset, sizeof(oldOffset));
            formatOffset(Location::offsetForIndex(locationDraftCity,
                                                 now.year, now.month, now.day),
                         newOffset, sizeof(newOffset));
            lv_label_set_text(reviewTitle, "Confirm changes");
            lv_label_set_text(reviewLabel, "LOCATION");
            lv_label_set_text(reviewValue, Location::at(locationDraftCity).name);
            snprintf(oldNew, sizeof(oldNew), "%s  >  %s  |  %s",
                     Location::city(), Location::at(locationDraftCity).name,
                     newOffset);
            lv_label_set_text(reviewOldNew, oldNew);
            break;
        }
        }
        lv_label_set_text(reviewStatus, "PREVIEW  |  NOT SAVED");
    }

    void refreshTick()
    {
        char detail[48];
        if (draftTick) {
            lv_label_set_text(tickStateLabel, "Audible");
            snprintf(detail, sizeof(detail), "1 tick / second  |  ON");
            lv_label_set_text(tickDetailLabel, detail);
            lv_label_set_text(tickToggleLabel, "Mute");
            lv_obj_set_style_border_color(tickToggleCard,
                                          lv_color_hex(0xB06A8A), 0);
        } else {
            lv_label_set_text(tickStateLabel, "Muted");
            snprintf(detail, sizeof(detail), "1 tick / second  |  OFF");
            lv_label_set_text(tickDetailLabel, detail);
            lv_label_set_text(tickToggleLabel, "Unmute");
            lv_obj_set_style_border_color(tickToggleCard,
                                          lv_color_hex(0x5A8DEE), 0);
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

    void showChoice(ChoiceMode mode)
    {
        choiceMode = mode;
        currentPage = PAGE_CHOICE;
        refreshChoice();
        loadPage(choiceScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showEditorPage(bool initialize)
    {
        currentPage = PAGE_EDITOR;
        if (initialize) {
            Clock::DateTime now = Clock::now();
            draftDateTime = now;
            originalDateTime = now;
            if (editorMode == EDIT_BRIGHTNESS) {
                originalBrightness = Brightness::current();
                draftBrightness = originalBrightness;
                previewActive = true;
            }
        }
        refreshEditor();
        loadPage(editorScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showReview(ReviewMode mode)
    {
        reviewMode = mode;
        currentPage = PAGE_REVIEW;
        refreshReview();
        loadPage(reviewScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    int letterOfCity(int index)
    {
        const char *name = Location::at(index).name;
        char c = (name && name[0]) ? name[0] : 'A';
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 'A' || c > 'Z') c = 'A';
        return c - 'A';
    }

    bool cityMatchesLetter(int index)
    {
        return letterOfCity(index) == locationLetterFilter;
    }

    void rebuildLocationCities()
    {
        locationCityCount = 0;
        locationCityOptions[0] = '\0';

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

        if (locationCityCount == 0) {
            snprintf(locationCityOptions, sizeof(locationCityOptions),
                     "No cities");
        } else {
            size_t used = 0;
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

    int cityRollerIndexFor(int cityIndex)
    {
        for (int i = 0; i < locationCityCount; i++) {
            if (locationCityIndices[i] == cityIndex) return i;
        }
        return -1;
    }

    void syncLocationRollers()
    {
        locationSyncing = true;
        lv_roller_set_selected(locationLetterRoller,
                               (uint16_t)locationLetterFilter, LV_ANIM_OFF);
        rebuildLocationCities();

        int target = cityRollerIndexFor(locationDraftCity);
        if (target < 0 && locationCityCount > 0) {
            target = 0;
            locationDraftCity = locationCityIndices[0];
            lastCityForLetter[locationLetterFilter] = locationDraftCity;
        }
        if (locationCityCount > 0) {
            lv_roller_set_selected(locationCityRoller, (uint16_t)target,
                                   LV_ANIM_OFF);
        }
        locationReviewEnabled = (locationCityCount > 0);
        if (locationReview) {
            lv_obj_set_style_opa(locationReview,
                                 locationReviewEnabled ? 255 : 100, 0);
        }
        locationSyncing = false;
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
            locationDraftCity = locationCityIndices[target];
            lastCityForLetter[locationLetterFilter] = locationDraftCity;
            lv_roller_set_selected(locationCityRoller, (uint16_t)target,
                                   LV_ANIM_OFF);
        }
        locationReviewEnabled = (locationCityCount > 0);
        if (locationReview) {
            lv_obj_set_style_opa(locationReview,
                                 locationReviewEnabled ? 255 : 100, 0);
        }
        locationSyncing = false;
    }

    void onLocationCityChanged(lv_event_t *event)
    {
        if (locationSyncing) return;

        int selected = (int)lv_roller_get_selected(lv_event_get_target(event));
        if (selected >= 0 && selected < locationCityCount) {
            locationDraftCity = locationCityIndices[selected];
            lastCityForLetter[locationLetterFilter] = locationDraftCity;
        }
    }

    void showLocationPage(bool initialize)
    {
        currentPage = PAGE_LOCATION;
        if (initialize) {
            locationDraftCity = Location::index();
            locationLetterFilter = letterOfCity(locationDraftCity);
            for (int i = 0; i < 26; i++) lastCityForLetter[i] = -1;
            lastCityForLetter[locationLetterFilter] = locationDraftCity;
        }
        syncLocationRollers();
        loadPage(locationScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void refreshTickScreen()
    {
        refreshTick();
        loadPage(tickScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showTickPage(bool initialize)
    {
        currentPage = PAGE_TICK;
        if (initialize) {
            originalTick = Audio::enabled();
            draftTick = originalTick;
            previewActive = true;
        }
        refreshTick();
        loadPage(tickScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void showDiscard(Page source, Page returnPage)
    {
        discardSource = source;
        discardReturn = returnPage;
        currentPage = PAGE_DISCARD;
        if (source == PAGE_EDITOR) {
            lv_label_set_text(discardMessage, "Discard this edit?");
        } else if (source == PAGE_LOCATION) {
            lv_label_set_text(discardMessage, "Discard city change?");
        } else if (source == PAGE_TICK) {
            lv_label_set_text(discardMessage, "Discard tick sound change?");
        }
        loadPage(discardScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }

    void discardKeep()
    {
        if (discardSource == PAGE_EDITOR) showEditorPage(false);
        else if (discardSource == PAGE_LOCATION) showLocationPage(false);
        else if (discardSource == PAGE_TICK) showTickPage(false);
    }

    void discardConfirm()
    {
        if (discardSource == PAGE_EDITOR && editorMode == EDIT_BRIGHTNESS) {
            Brightness::set(originalBrightness);
        }
        if (discardSource == PAGE_TICK) {
            Audio::setEnabled(originalTick, false);
        }
        previewActive = false;

        if (discardReturn == PAGE_CHOICE) showChoice(choiceMode);
        else showOverview();
    }

    void requestEditorBack()
    {
        if (editorDirty()) {
            showDiscard(PAGE_EDITOR, PAGE_CHOICE);
        } else {
            previewActive = false;
            showChoice(choiceMode);
        }
    }

    void requestLocationBack()
    {
        if (locationDraftCity != Location::index()) {
            showDiscard(PAGE_LOCATION, PAGE_OVERVIEW);
        } else {
            showOverview();
        }
    }

    void requestTickBack()
    {
        if (draftTick != originalTick) {
            showDiscard(PAGE_TICK, PAGE_CHOICE);
        } else {
            previewActive = false;
            showChoice(CHOICE_DISPLAY);
        }
    }

    void requestReviewBack()
    {
        if (reviewMode == REVIEW_LOCATION) showLocationPage(false);
        else showEditorPage(false);
    }

    void adjustEditor(int delta)
    {
        if (editorMode == EDIT_TIME) {
            if (editStep == STEP_HOUR) {
                draftDateTime.hour = (draftDateTime.hour + delta + 24) % 24;
            } else {
                draftDateTime.minute =
                    (draftDateTime.minute + delta + 60) % 60;
            }
        } else if (editorMode == EDIT_DATE) {
            if (editStep == STEP_YEAR) {
                draftDateTime.year += delta;
                if (draftDateTime.year < 2020) draftDateTime.year = 2020;
                if (draftDateTime.year > 2069) draftDateTime.year = 2069;
                int maxDay = Clock::daysInMonth(draftDateTime.year,
                                                draftDateTime.month);
                if (draftDateTime.day > maxDay) draftDateTime.day = maxDay;
            } else if (editStep == STEP_MONTH) {
                int month = draftDateTime.month + delta;
                if (month < 1) month = 12;
                if (month > 12) month = 1;
                draftDateTime.month = month;
                int maxDay = Clock::daysInMonth(draftDateTime.year,
                                                draftDateTime.month);
                if (draftDateTime.day > maxDay) draftDateTime.day = maxDay;
            } else {
                draftDateTime.day += delta;
                int maxDay = Clock::daysInMonth(draftDateTime.year,
                                                draftDateTime.month);
                if (draftDateTime.day < 1) draftDateTime.day = 1;
                if (draftDateTime.day > maxDay) draftDateTime.day = maxDay;
            }
        } else {
            draftBrightness += delta * 5;
            /* Keep the panel readable; lower values are unsafe on this LCD. */
            if (draftBrightness < 40) draftBrightness = 40;
            if (draftBrightness > 100) draftBrightness = 100;
            Brightness::set(draftBrightness);
            previewActive = true;
        }
        refreshEditor();
    }

    void editorNext()
    {
        if (editorMode == EDIT_TIME) {
            if (editStep == STEP_HOUR) {
                editStep = STEP_MINUTE;
                refreshEditor();
            } else {
                showReview(REVIEW_TIME);
            }
        } else if (editorMode == EDIT_DATE) {
            if (editStep == STEP_YEAR) {
                editStep = STEP_MONTH;
                refreshEditor();
            } else if (editStep == STEP_MONTH) {
                editStep = STEP_DAY;
                refreshEditor();
            } else {
                showReview(REVIEW_DATE);
            }
        } else {
            previewActive = false;
            Brightness::set(draftBrightness);
            SettingsStore::save();
            showOverview();
        }
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

    void reviewSave()
    {
        if (saving) return;
        saving = true;

        bool ok = false;
        if (reviewMode == REVIEW_LOCATION) {
            ok = applyLocation(locationDraftCity);
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
            previewActive = false;
            showOverview();
        } else {
            lv_label_set_text(reviewStatus, "SAVE FAILED  |  RETRY");
        }
    }

    void reviewCancel()
    {
        previewActive = false;
        showOverview();
    }

    void createOverview()
    {
        overviewScreen = make_screen();
        make_label(overviewScreen, 12, 8, 240, 32, "Settings",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);
        overviewBatLabel = make_label(overviewScreen, 380, 18, 248, 18, "",
                                      &ui_font_Inter15M, COLOR_MUTED,
                                      LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *dateCard = make_card(overviewScreen, 12, 54, 200, 106, true);
        lv_obj_add_event_cb(dateCard, [](lv_event_t *) {
            showChoice(CHOICE_DATETIME);
        }, LV_EVENT_CLICKED, NULL);
        make_label(dateCard, 12, 10, 170, 12, "DATE & TIME",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        overviewTime = make_label(dateCard, 12, 22, 124, 50, "",
                                  &ui_font_Inter42, COLOR_TEXT,
                                  LV_TEXT_ALIGN_LEFT);
        overviewSeconds = make_label(dateCard, 136, 50, 40, 20, "",
                                     &ui_font_Inter18, COLOR_MUTED,
                                     LV_TEXT_ALIGN_LEFT);
        overviewDate = make_label(dateCard, 12, 78, 170, 18, "",
                                  &ui_font_Inter18, COLOR_MUTED,
                                  LV_TEXT_ALIGN_LEFT);
        make_label(dateCard, 168, 44, 20, 20, "\xEF\x81\x94",
                   &ui_font_FA14, COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);

        lv_obj_t *cityCard = make_card(overviewScreen, 220, 54, 200, 106, true);
        lv_obj_add_event_cb(cityCard, [](lv_event_t *) {
            showLocationPage(true);
        }, LV_EVENT_CLICKED, NULL);
        make_label(cityCard, 12, 10, 170, 12, "LOCATION",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        overviewCity = make_label(cityCard, 12, 24, 170, 34, "",
                                  &ui_font_Inter28, COLOR_TEXT,
                                  LV_TEXT_ALIGN_LEFT);
        overviewOffset = make_label(cityCard, 12, 78, 170, 18, "",
                                    &ui_font_Inter18, COLOR_MUTED,
                                    LV_TEXT_ALIGN_LEFT);
        make_label(cityCard, 168, 44, 20, 20, "\xEF\x81\x94",
                   &ui_font_FA14, COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);

        lv_obj_t *displayCard = make_card(overviewScreen, 428, 54, 200, 106,
                                          true);
        lv_obj_add_event_cb(displayCard, [](lv_event_t *) {
            showChoice(CHOICE_DISPLAY);
        }, LV_EVENT_CLICKED, NULL);
        make_label(displayCard, 12, 10, 170, 12, "DISPLAY",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        overviewBrightness = make_label(displayCard, 12, 22, 170, 50, "",
                                        &ui_font_Inter42, COLOR_TEXT,
                                        LV_TEXT_ALIGN_LEFT);
        make_label(displayCard, 12, 78, 170, 18, "Brightness",
                   &ui_font_Inter18, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        make_label(displayCard, 168, 44, 20, 20, "\xEF\x81\x94",
                   &ui_font_FA14, COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    }

    void createChoice()
    {
        choiceScreen = make_screen();
        make_back_button(choiceScreen, [](lv_event_t *) { showOverview(); });
        choiceTitle = make_label(choiceScreen, 110, 12, 300, 32, "",
                                 &ui_font_Inter28, COLOR_TEXT,
                                 LV_TEXT_ALIGN_LEFT);
        choiceModeLabel = make_label(choiceScreen, 420, 18, 208, 18, "",
                                     &ui_font_Inter15M, COLOR_MUTED,
                                     LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *card1 = make_card(choiceScreen, 12, 57, 300, 100, true);
        lv_obj_add_event_cb(card1, [](lv_event_t *) {
            if (choiceMode == CHOICE_DATETIME) {
                editorMode = EDIT_TIME;
                editStep = STEP_HOUR;
                showEditorPage(true);
            } else {
                editorMode = EDIT_BRIGHTNESS;
                editStep = STEP_BRIGHTNESS;
                showEditorPage(true);
            }
        }, LV_EVENT_CLICKED, NULL);
        choiceCard1Label = make_label(card1, 12, 10, 250, 14, "",
                                      &ui_font_Inter10M, COLOR_MUTED,
                                      LV_TEXT_ALIGN_LEFT);
        choiceCard1Value = make_label(card1, 12, 28, 250, 48, "",
                                      &ui_font_Inter42, COLOR_TEXT,
                                      LV_TEXT_ALIGN_LEFT);
        choiceCard1Sub = make_label(card1, 12, 76, 250, 18, "",
                                    &ui_font_Inter18, COLOR_MUTED,
                                    LV_TEXT_ALIGN_LEFT);
        make_label(card1, 270, 38, 20, 20, "\xEF\x81\x94",
                   &ui_font_FA14, COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);

        lv_obj_t *card2 = make_card(choiceScreen, 320, 57, 300, 100, true);
        lv_obj_add_event_cb(card2, [](lv_event_t *) {
            if (choiceMode == CHOICE_DATETIME) {
                editorMode = EDIT_DATE;
                editStep = STEP_YEAR;
                showEditorPage(true);
            } else {
                showTickPage(true);
            }
        }, LV_EVENT_CLICKED, NULL);
        choiceCard2Label = make_label(card2, 12, 10, 250, 14, "",
                                      &ui_font_Inter10M, COLOR_MUTED,
                                      LV_TEXT_ALIGN_LEFT);
        choiceCard2Value = make_label(card2, 12, 28, 250, 48, "",
                                      &ui_font_Inter42, COLOR_TEXT,
                                      LV_TEXT_ALIGN_LEFT);
        choiceCard2Sub = make_label(card2, 12, 76, 250, 18, "",
                                    &ui_font_Inter18, COLOR_MUTED,
                                    LV_TEXT_ALIGN_LEFT);
        make_label(card2, 270, 38, 20, 20, "\xEF\x81\x94",
                   &ui_font_FA14, COLOR_ACCENT, LV_TEXT_ALIGN_CENTER);
    }

    void createEditor()
    {
        editorScreen = make_screen();
        make_back_button(editorScreen, [](lv_event_t *) { requestEditorBack(); });
        editorTitle = make_label(editorScreen, 110, 12, 300, 32, "",
                                 &ui_font_Inter28, COLOR_TEXT,
                                 LV_TEXT_ALIGN_LEFT);
        editorStepLabel = make_label(editorScreen, 400, 18, 228, 18, "",
                                     &ui_font_Inter15M, COLOR_MUTED,
                                     LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *minusCard = make_card(editorScreen, 12, 57, 108, 100, true);
        lv_obj_add_event_cb(minusCard, [](lv_event_t *) {
            adjustEditor(-1);
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *minusLine = lv_obj_create(minusCard);
        lv_obj_remove_style_all(minusLine);
        lv_obj_set_size(minusLine, 44, 5);
        lv_obj_align(minusLine, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(minusLine, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_bg_opa(minusLine, 255, 0);
        lv_obj_set_style_radius(minusLine, 2, 0);
        lv_obj_clear_flag(minusLine,
                          LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *valueCard = make_card(editorScreen, 130, 57, 264, 100);
        editorFieldLabel = make_label(valueCard, 0, 8, 264, 16, "",
                                      &ui_font_Inter15M, COLOR_MUTED,
                                      LV_TEXT_ALIGN_CENTER);
        editorValueLabel = make_label(valueCard, 0, 26, 264, 52, "",
                                      &ui_font_Inter42, COLOR_TEXT,
                                      LV_TEXT_ALIGN_CENTER);
        editorSubLabel = make_label(valueCard, 0, 80, 264, 14, "",
                                    &ui_font_Inter10M, COLOR_MUTED,
                                    LV_TEXT_ALIGN_CENTER);
        make_action_card(editorScreen, 408, 57, 108, 100, "+",
                         &ui_font_Inter42, COLOR_CARD, 0, COLOR_TEXT,
                         [](lv_event_t *) { adjustEditor(1); });
        lv_obj_t *action = make_action_card(editorScreen, 528, 57, 100, 100,
                                            "", &ui_font_Inter18,
                                            COLOR_ACTION, COLOR_ACTION_EDGE,
                                            COLOR_TEXT,
                                            [](lv_event_t *) { editorNext(); });
        editorActionLabel = lv_obj_get_child(action, 0);
    }

    void createReview()
    {
        reviewScreen = make_screen();
        make_back_button(reviewScreen, [](lv_event_t *) { requestReviewBack(); });
        reviewTitle = make_label(reviewScreen, 110, 12, 300, 32, "",
                                 &ui_font_Inter28, COLOR_TEXT,
                                 LV_TEXT_ALIGN_LEFT);
        reviewStatus = make_label(reviewScreen, 400, 18, 228, 18, "",
                                  &ui_font_Inter15M, COLOR_MUTED,
                                  LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *card = make_card(reviewScreen, 12, 57, 416, 100);
        reviewLabel = make_label(card, 12, 10, 200, 14, "",
                                 &ui_font_Inter10M, COLOR_MUTED,
                                 LV_TEXT_ALIGN_LEFT);
        reviewValue = make_label(card, 12, 26, 390, 50, "",
                                 &ui_font_Inter42, COLOR_TEXT,
                                 LV_TEXT_ALIGN_LEFT);
        reviewOldNew = make_label(card, 12, 78, 390, 18, "",
                                  &ui_font_Inter18, COLOR_MUTED,
                                  LV_TEXT_ALIGN_LEFT);

        make_action_card(reviewScreen, 436, 57, 90, 100, "Cancel",
                         &ui_font_Inter18, COLOR_CARD, 0, COLOR_TEXT,
                         [](lv_event_t *) { reviewCancel(); });
        make_action_card(reviewScreen, 528, 57, 100, 100, "Save",
                         &ui_font_Inter18, COLOR_ACTION, COLOR_ACTION_EDGE,
                         COLOR_TEXT, [](lv_event_t *) { reviewSave(); });
    }

    void style_location_roller(lv_obj_t *roller, lv_text_align_t align)
    {
        lv_obj_set_style_bg_opa(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, &ui_font_Inter28, LV_PART_MAIN);
        lv_obj_set_style_text_color(roller, lv_color_hex(0x7F8A99),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_align(roller, align, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(roller, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x3A5A7A),
                                  LV_PART_SELECTED);
        lv_obj_set_style_bg_opa(roller, 255, LV_PART_SELECTED);
        lv_obj_set_style_text_color(roller, lv_color_hex(COLOR_TEXT),
                                    LV_PART_SELECTED);
        lv_obj_set_style_radius(roller, 10, LV_PART_SELECTED);
    }

    void createLocation()
    {
        locationScreen = make_screen();
        make_back_button(locationScreen, [](lv_event_t *) { requestLocationBack(); });
        locationTitle = make_label(locationScreen, 110, 12, 300, 32,
                                   "LOCATION", &ui_font_Inter28,
                                   COLOR_TEXT, LV_TEXT_ALIGN_LEFT);
        make_label(locationScreen, 420, 18, 208, 18, "CITY SETTINGS",
                   &ui_font_Inter15M, COLOR_MUTED, LV_TEXT_ALIGN_RIGHT);

        make_label(locationScreen, 14, 50, 100, 14, "INITIAL",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        make_label(locationScreen, 160, 50, 100, 14, "CITY",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);

        locationLetterRoller = lv_roller_create(locationScreen);
        style_location_roller(locationLetterRoller, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_pos(locationLetterRoller, 14, 66);
        lv_obj_set_width(locationLetterRoller, 130);
        lv_roller_set_options(locationLetterRoller, locationLetterOptions,
                              LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationLetterRoller, 3);
        lv_obj_add_event_cb(locationLetterRoller, onLocationLetterChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);

        locationCityRoller = lv_roller_create(locationScreen);
        style_location_roller(locationCityRoller, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(locationCityRoller, 160, 66);
        lv_obj_set_width(locationCityRoller, 326);
        lv_roller_set_options(locationCityRoller, "", LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(locationCityRoller, 3);
        lv_obj_add_event_cb(locationCityRoller, onLocationCityChanged,
                            LV_EVENT_VALUE_CHANGED, NULL);

        locationReview = make_action_card(locationScreen, 498, 66, 128, 100,
                                          "REVIEW\n>",
                                          &ui_font_Inter18, COLOR_CARD, 0,
                                          COLOR_TEXT, [](lv_event_t *) {
                                              if (locationReviewEnabled) {
                                                  showReview(REVIEW_LOCATION);
                                              }
                                          });
    }

    void createTick()
    {
        tickScreen = make_screen();
        make_back_button(tickScreen, [](lv_event_t *) { requestTickBack(); });
        tickTitle = make_label(tickScreen, 110, 12, 300, 32, "Tick sound",
                               &ui_font_Inter28, COLOR_TEXT,
                               LV_TEXT_ALIGN_LEFT);
        make_label(tickScreen, 420, 18, 208, 18, "SETTINGS",
                   &ui_font_Inter15M, COLOR_MUTED, LV_TEXT_ALIGN_RIGHT);

        lv_obj_t *card = make_card(tickScreen, 12, 57, 300, 100);
        make_label(card, 12, 10, 276, 14, "TICK AUDIO",
                   &ui_font_Inter10M, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        tickStateLabel = make_label(card, 12, 26, 276, 50, "",
                                    &ui_font_Inter42, COLOR_TEXT,
                                    LV_TEXT_ALIGN_LEFT);
        tickDetailLabel = make_label(card, 12, 78, 276, 18, "",
                                     &ui_font_Inter18, COLOR_MUTED,
                                     LV_TEXT_ALIGN_LEFT);

        tickToggleCard = make_card(tickScreen, 320, 57, 150, 100, true);
        lv_obj_set_style_border_width(tickToggleCard, 2, 0);
        lv_obj_add_event_cb(tickToggleCard, [](lv_event_t *) {
            draftTick = !draftTick;
            Audio::setEnabled(draftTick, false);
            previewActive = true;
            refreshTick();
        }, LV_EVENT_CLICKED, NULL);
        tickToggleLabel = make_label(tickToggleCard, 0, 28, 150, 32, "",
                                     &ui_font_Inter28, COLOR_TEXT,
                                     LV_TEXT_ALIGN_CENTER);
        tickToggleSub = make_label(tickToggleCard, 0, 64, 150, 18, "Preview",
                                   &ui_font_Inter15M, COLOR_MUTED,
                                   LV_TEXT_ALIGN_CENTER);

        make_action_card(tickScreen, 480, 57, 148, 100, "Save",
                         &ui_font_Inter28, COLOR_ACTION, COLOR_ACTION_EDGE,
                         COLOR_TEXT, [](lv_event_t *) {
                             Audio::setEnabled(draftTick, true);
                             previewActive = false;
                             SettingsStore::save();
                             showOverview();
                         });
    }

    void createDiscard()
    {
        discardScreen = make_screen();
        make_label(discardScreen, 12, 8, 400, 32, "Discard changes?",
                   &ui_font_Inter28, COLOR_TEXT, LV_TEXT_ALIGN_LEFT);
        discardMessage = make_label(discardScreen, 12, 54, 400, 24, "",
                                    &ui_font_Inter18, COLOR_MUTED,
                                    LV_TEXT_ALIGN_LEFT);
        make_action_card(discardScreen, 12, 92, 300, 64, "Keep editing",
                         &ui_font_Inter18, COLOR_CARD, 0, COLOR_TEXT,
                         [](lv_event_t *) { discardKeep(); });
        make_action_card(discardScreen, 328, 92, 300, 64, "Discard",
                         &ui_font_Inter18, 0x5A3038, 0xB06A8A, COLOR_TEXT,
                         [](lv_event_t *) { discardConfirm(); });
    }
}

namespace Settings
{
    void begin()
    {
        if (overviewScreen) return;

        createOverview();
        createChoice();
        createEditor();
        createReview();
        createLocation();
        createTick();
        createDiscard();

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

        if (previewActive) {
            if ((currentPage == PAGE_EDITOR &&
                 editorMode == EDIT_BRIGHTNESS) ||
                (currentPage == PAGE_DISCARD &&
                 discardSource == PAGE_EDITOR &&
                 editorMode == EDIT_BRIGHTNESS)) {
                Brightness::set(originalBrightness);
            }
            if (currentPage == PAGE_TICK ||
                (currentPage == PAGE_DISCARD &&
                 discardSource == PAGE_TICK)) {
                Audio::setEnabled(originalTick, false);
            }
            previewActive = false;
        }

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
        return previewActive;
    }

    void showLocationPicker()
    {
        if (!openFlag) return;
        showLocationPage(true);
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
        if (!openFlag) return;
        showChoice(CHOICE_DATETIME);
    }

    void hideDateTimePicker(bool)
    {
        if (currentPage == PAGE_CHOICE && choiceMode == CHOICE_DATETIME) {
            showOverview();
        }
    }

    bool isDateTimePickerOpen()
    {
        return currentPage == PAGE_CHOICE && choiceMode == CHOICE_DATETIME;
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
        if (applyLocation(index) && openFlag) {
            refreshOverview();
        }
    }

    void debugOpen(int page)
    {
        if (!openFlag) return;

        switch (page) {
        case 0:
            showOverview();
            break;
        case 1:
            showChoice(CHOICE_DATETIME);
            break;
        case 2:
            editorMode = EDIT_TIME;
            editStep = STEP_HOUR;
            showEditorPage(true);
            break;
        case 3:
            editorMode = EDIT_DATE;
            editStep = STEP_YEAR;
            showEditorPage(true);
            break;
        case 4: {
            Clock::DateTime now = Clock::now();
            draftDateTime = now;
            originalDateTime = now;
            showReview(REVIEW_TIME);
            break;
        }
        case 5:
            showChoice(CHOICE_DISPLAY);
            break;
        case 6:
            editorMode = EDIT_BRIGHTNESS;
            editStep = STEP_BRIGHTNESS;
            showEditorPage(true);
            break;
        case 7:
            showTickPage(true);
            break;
        case 8:
            showLocationPage(true);
            break;
        default:
            break;
        }
    }
}
