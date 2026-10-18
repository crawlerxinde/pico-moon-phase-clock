#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Native panel geometry.  The panel is rotated to landscape by LCD_Init(). */
#define LCD_PANEL_WIDTH   172
#define LCD_PANEL_HEIGHT  640

/* GPIO assignments from the Waveshare RP2350-Touch-LCD-3.49 schematic. */
#define LCD_BL_PIN        36
#define LCD_PWR_EN_PIN    37

void LCD_Init(void);
void LCD_SetBacklight(uint8_t percent);
void LCD_SetRotation(uint8_t madctl);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_WritePixels(uint16_t *pixels, size_t count);

/* Full-panel streaming write: set the 172x640 window once, then stream
 * blocks without re-sending the address window or 0x2C between blocks. */
void LCD_BeginFullWrite(void);
void LCD_WritePixelsStream(const uint16_t *pixels, size_t count);
void LCD_EndFullWrite(void);

#ifdef __cplusplus
}
#endif
