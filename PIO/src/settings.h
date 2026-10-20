#pragma once

namespace Settings
{
    void begin();
    void update();

    /* A negative swipeDx slides the incoming page from the right; a positive
     * one slides it from the left.  Zero keeps the default animation. */
    void show(int swipeDx = 0);
    void hide(int swipeDx = 0);
    bool isOpen();

    /* Full-screen city picker opened from the Location card. */
    void showLocationPicker();
    void hideLocationPicker(int swipeDx = 0);
    bool isLocationPickerOpen();

    /* Full-screen date/time editor opened from the date/time pills. */
    void showDateTimePicker();
    void hideDateTimePicker(bool apply);
    bool isDateTimePickerOpen();

    /* True while a Settings control is being pressed (or just released), so
     * the clock-page swipe detector does not close the page by accident. */
    bool swipeBlocked();

    /* True while brightness or tick sound is being previewed, so the
     * periodic settings snapshot does not persist a draft value. */
    bool isPreviewActive();

    /* Change the active city and keep the absolute UTC instant unchanged.
     * The RTC is rewritten in the new city's local time. */
    void selectLocation(int index);

    /* Select a date/time field (0=year..5=second) and adjust it. */
    void adjustField(int field, int delta);

    /* Diagnostic screen navigation used by the serial Z command. */
    void debugOpen(int page);
}
