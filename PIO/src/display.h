#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace Display
{
    /* Logical LVGL resolution.  The panel itself is driven in its native
     * 172x640 portrait orientation and rotated by the flush callback. */
    constexpr uint16_t Width  = 640;
    constexpr uint16_t Height = 172;

    /* Touch calibration.  The AXS15231B reports raw coordinates in a
     * 640x172 landscape frame.  These are runtime flags so they can also be
     * changed over the serial console (X / Y / S commands). */
    extern bool TouchSwapXY;
    extern bool TouchInvertX;
    extern bool TouchInvertY;

    /* Last valid touch sample, refreshed by the LVGL indev callback.
     * Settings uses this for manual swipe detection. */
    extern bool     TouchDown;
    extern uint16_t TouchLastX;
    extern uint16_t TouchLastY;

    void begin();

    /* Read one touch sample (mapped logical coordinates).  When rawX/rawY are
     * supplied they receive the controller's raw values before calibration;
     * used by the serial touch diagnostic. */
    bool readTouch(uint16_t &x, uint16_t &y,
                   uint16_t *rawX = nullptr, uint16_t *rawY = nullptr);

    /* Full-screen draw buffer, also used as the screenshot source buffer. */
    uint8_t *screenBuffer();
    uint32_t screenBufferSize();

    /* Software 180-degree display flip (the panel stays in portrait mode). */
    void setFlipped(bool flipped);
    bool flipped();

    /* Fill the native panel with one RGB565 colour. */
    void clearPanel(uint16_t color);

    /* Temporary hardware diagnostics. */
    void fillPanelPortrait();
    void fillPanelLandscape();
    uint32_t flushCount();
    uint16_t lastFlushWidth();
    uint16_t lastFlushHeight();
}
