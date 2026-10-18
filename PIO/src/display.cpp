#include "display.h"

#include <Arduino.h>
#include <lvgl.h>

#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include "lcd_3in49.h"

namespace
{
    constexpr uint8_t  TOUCH_I2C_ADDR = 0x3B;
    constexpr int      TOUCH_SDA_PIN  = 32;
    constexpr int      TOUCH_SCL_PIN  = 33;
    constexpr uint32_t TOUCH_I2C_HZ   = 400000;
    constexpr uint32_t DRAW_BUF_PIXELS = Display::Width * Display::Height;
    constexpr int      ROT_BLOCK_ROWS  = 8;

    lv_disp_draw_buf_t draw_buf;
    lv_color_t         draw_buf_mem[DRAW_BUF_PIXELS];
    uint16_t           rot_buf[LCD_PANEL_WIDTH * ROT_BLOCK_ROWS];
    uint16_t           test_buf[Display::Width * ROT_BLOCK_ROWS];
    bool               screenFlipped = false;
    uint32_t           flushCountValue = 0;
    uint16_t           lastFlushW = 0;
    uint16_t           lastFlushH = 0;

    inline uint16_t panelColor(uint16_t rgb565)
    {
        return (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
    }

    uint8_t touch_cmd[11] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00};

    void touch_begin()
    {
        i2c_init(i2c0, TOUCH_I2C_HZ);
        gpio_set_function(TOUCH_SDA_PIN, GPIO_FUNC_I2C);
        gpio_set_function(TOUCH_SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(TOUCH_SDA_PIN);
        gpio_pull_up(TOUCH_SCL_PIN);
    }

    bool touch_read(uint16_t &x, uint16_t &y)
    {
        uint8_t data[32] = {0};

        if (i2c_write_blocking(i2c0, TOUCH_I2C_ADDR, touch_cmd, sizeof(touch_cmd), true) < 0) {
            return false;
        }
        if (i2c_read_blocking(i2c0, TOUCH_I2C_ADDR, data, sizeof(data), false) < 0) {
            return false;
        }

        if (data[1] == 0) {
            return false;
        }

        uint16_t raw_x = (((uint16_t)data[2] & 0x0F) << 8) | data[3];
        uint16_t raw_y = (((uint16_t)data[4] & 0x0F) << 8) | data[5];

        if (Display::TouchSwapXY) {
            uint16_t t = raw_x;
            raw_x = raw_y;
            raw_y = t;
        }

        if (raw_x >= Display::Width)  raw_x = Display::Width - 1;
        if (raw_y >= Display::Height) raw_y = Display::Height - 1;

        x = Display::TouchInvertX ? (Display::Width  - 1 - raw_x) : raw_x;
        y = Display::TouchInvertY ? (Display::Height - 1 - raw_y) : raw_y;
        return true;
    }

    void touch_read_cb(lv_indev_drv_t *, lv_indev_data_t *data)
    {
        uint16_t x = 0, y = 0;
        if (touch_read(x, y)) {
            data->point.x = x;
            data->point.y = y;
            data->state   = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }

    void flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
    {
        const int32_t x1 = area->x1;
        const int32_t x2 = area->x2;
        const int32_t y1 = area->y1;
        const int32_t y2 = area->y2;
        const uint32_t w = (uint32_t)(x2 - x1 + 1);
        const uint32_t h = (uint32_t)(y2 - y1 + 1);

        flushCountValue++;
        lastFlushW = (uint16_t)w;
        lastFlushH = (uint16_t)h;

        /* Full-refresh rotation matching pico-clock:
         * logical 640x172 -> panel native 172x640, one full-panel window,
         * continuous pixel stream. */
        const uint16_t *src = (const uint16_t *)color_p;

        LCD_BeginFullWrite();

        for (int y = 0; y < LCD_PANEL_HEIGHT; y += ROT_BLOCK_ROWS) {
            int rows = LCD_PANEL_HEIGHT - y;
            if (rows > ROT_BLOCK_ROWS) rows = ROT_BLOCK_ROWS;

            for (int r = 0; r < rows; r++) {
                const int py = y + r;
                for (int px = 0; px < LCD_PANEL_WIDTH; px++) {
                    uint16_t value;
                    if (screenFlipped) {
                        value = src[px * Display::Width +
                                    (Display::Width - 1 - py)];
                    } else {
                        value = src[(LCD_PANEL_WIDTH - 1 - px) * Display::Width + py];
                    }
                    rot_buf[r * LCD_PANEL_WIDTH + px] =
                        (uint16_t)((value >> 8) | (value << 8));
                }
            }

            LCD_WritePixelsStream(rot_buf, (size_t)rows * LCD_PANEL_WIDTH);
        }

        LCD_EndFullWrite();
        lv_disp_flush_ready(disp);
    }
}

namespace Display
{
    bool TouchSwapXY = false;
    /* With the software-rotated panel, the vendor touch mapping inverts both
     * axes.  X/Y/S can still override this at runtime. */
    bool TouchInvertX = true;
    bool TouchInvertY = true;

    uint8_t *screenBuffer()
    {
        return (uint8_t *)draw_buf_mem;
    }

    uint32_t screenBufferSize()
    {
        return (uint32_t)sizeof(draw_buf_mem);
    }

    void setFlipped(bool flipped)
    {
        screenFlipped = flipped;
    }

    bool flipped()
    {
        return screenFlipped;
    }

    uint32_t flushCount()
    {
        return flushCountValue;
    }

    uint16_t lastFlushWidth()
    {
        return lastFlushW;
    }

    uint16_t lastFlushHeight()
    {
        return lastFlushH;
    }

    void clearPanel(uint16_t color)
    {
        const uint16_t value = panelColor(color);
        for (int i = 0; i < LCD_PANEL_WIDTH * ROT_BLOCK_ROWS; i++) {
            rot_buf[i] = value;
        }

        LCD_BeginFullWrite();
        for (int y = 0; y < LCD_PANEL_HEIGHT; y += ROT_BLOCK_ROWS) {
            int rows = LCD_PANEL_HEIGHT - y;
            if (rows > ROT_BLOCK_ROWS) rows = ROT_BLOCK_ROWS;
            LCD_WritePixelsStream(rot_buf, (size_t)rows * LCD_PANEL_WIDTH);
        }
        LCD_EndFullWrite();
    }

    void fillPanelPortrait()
    {
        for (int y = 0; y < LCD_PANEL_HEIGHT; y += ROT_BLOCK_ROWS) {
            int rows = LCD_PANEL_HEIGHT - y;
            if (rows > ROT_BLOCK_ROWS) rows = ROT_BLOCK_ROWS;
            for (int r = 0; r < rows; r++) {
                int py = y + r;
                for (int px = 0; px < LCD_PANEL_WIDTH; px++) {
                    uint16_t color;
                    if (px < LCD_PANEL_WIDTH / 2) {
                        color = (py < LCD_PANEL_HEIGHT / 2) ? 0xF800 : 0x001F;
                    } else {
                        color = (py < LCD_PANEL_HEIGHT / 2) ? 0x07E0 : 0xFFFF;
                    }
                    rot_buf[r * LCD_PANEL_WIDTH + px] = panelColor(color);
                }
            }
            LCD_SetWindow(0, (uint16_t)y,
                          LCD_PANEL_WIDTH - 1, (uint16_t)(y + rows - 1));
            LCD_WritePixels(rot_buf, (size_t)rows * LCD_PANEL_WIDTH);
        }
    }

    void fillPanelLandscape()
    {
        constexpr int W = 640;
        constexpr int H = 172;
        for (int y = 0; y < H; y += ROT_BLOCK_ROWS) {
            int rows = H - y;
            if (rows > ROT_BLOCK_ROWS) rows = ROT_BLOCK_ROWS;
            for (int r = 0; r < rows; r++) {
                int py = y + r;
                for (int px = 0; px < W; px++) {
                    uint16_t color;
                    if (px < W / 2) {
                        color = (py < H / 2) ? 0xF800 : 0x001F;
                    } else {
                        color = (py < H / 2) ? 0x07E0 : 0xFFFF;
                    }
                    test_buf[r * W + px] = panelColor(color);
                }
            }
            LCD_SetWindow(0, (uint16_t)y, W - 1, (uint16_t)(y + rows - 1));
            LCD_WritePixels(test_buf, (size_t)rows * W);
        }
    }

    void begin()
    {
        lv_init();

        LCD_Init();
        clearPanel(0xFFFF);
        touch_begin();

        lv_disp_draw_buf_init(&draw_buf, draw_buf_mem, nullptr, DRAW_BUF_PIXELS);

        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res      = Width;
        disp_drv.ver_res      = Height;
        disp_drv.flush_cb     = flush_cb;
        disp_drv.draw_buf     = &draw_buf;
        disp_drv.full_refresh = 1;
        lv_disp_drv_register(&disp_drv);

        screenFlipped = false;

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type    = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = touch_read_cb;
        lv_indev_drv_register(&indev_drv);
    }
}
