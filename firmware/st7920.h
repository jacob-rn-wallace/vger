/**
 * @file st7920.h
 * @brief Low-level 8-bit parallel driver for the NHD-14432WG-BTFH-VT
 *        (ST7920 controller) 144x32 graphic LCD - vger's hardware target,
 *        see CLAUDE.md's "Hardware target" section.
 *
 * Ported from `soynut`'s hardware-validated `firmware/st7920.c`/`.h`
 * (same author; see DEVIATIONS.md for the GPL-2.0-to-Apache-2.0 license
 * note this port crosses). Real hardware confirmed this exact addressing
 * and command sequence via solid-fill and checkerboard test patterns
 * (`soynut/lcd_bringup/`) before it was adopted there - see
 * `soynut/CLAUDE.md`'s "Direct Pico->LCD parallel link" section for that
 * validation history. This header/its .c file are otherwise unmodified
 * from soynut's, other than renaming the public symbols onto vger's
 * `vger_`/`VGER_` naming convention.
 */
#ifndef VGER_ST7920_H
#define VGER_ST7920_H

#include <stdint.h>
#include <stddef.h>

// NHD-14432WG-BTFH-VT: 144 x 32 graphic LCD, ST7920 controller, wired
// 8-bit parallel (see pins.h / CLAUDE.md's "Hardware target" section).

#define VGER_LCD_WIDTH_PX   144
#define VGER_LCD_HEIGHT_PX  32
#define VGER_LCD_BYTES_PER_ROW (VGER_LCD_WIDTH_PX / 8)                        // 18
#define VGER_LCD_FB_SIZE       (VGER_LCD_BYTES_PER_ROW * VGER_LCD_HEIGHT_PX)  // 576

/**
 * @brief Configure GPIOs and run the ST7920 power-on init sequence.
 *
 * Must be called once before vger_st7920_clear()/vger_st7920_draw_frame().
 */
void vger_st7920_init(void);

/**
 * @brief Clear the controller's GDRAM directly (not just a local framebuffer).
 */
void vger_st7920_clear(void);

/**
 * @brief Push a full framebuffer to the controller's GDRAM.
 *
 * @param fb VGER_LCD_FB_SIZE bytes, MSB-first per row, row-major, 1bpp.
 */
void vger_st7920_draw_frame(const uint8_t *fb);

#endif // VGER_ST7920_H
