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

    /* True while a Settings control is being pressed (or just released), so
     * the clock-page swipe detector does not close the page by accident. */
    bool swipeBlocked();

    /* Change the active city and keep the absolute UTC instant unchanged.
     * The RTC is rewritten in the new city's local time. */
    void selectLocation(int index);

    /* Select a date/time field (0=year..5=second) and adjust it. */
    void adjustField(int field, int delta);
}
