/**
 * @file main.c
 * @brief Pico 2 firmware bring-up entry point (milestone 3, just started).
 *
 * Scope, deliberately narrow: prove the ported NHD14432/ST7920 driver
 * (st7920.c/.h, see their file comments for the porting history) and
 * `core/` both cross-compile and link for the real RP2350 target, and
 * exercise the driver with the same kind of solid-fill/checkerboard test
 * pattern soynut used to validate this exact display/wiring on real
 * hardware (see soynut/CLAUDE.md's "Direct Pico->LCD parallel link"
 * section) - not a preview of vger's real display output. Translating
 * `core/vger_state.h`'s query API into actual pixels needs a font/
 * segment-table layer that doesn't exist yet (see CLAUDE.md's "Deferred
 * work": the MENU/system-menu layer and its rendering are still ahead of
 * this), so this file intentionally never touches core/vger_interp.h.
 */

#include <stdint.h>

#include "pico/stdlib.h"

#include "st7920.h"
#include "vger_assert.h"

/** @brief Fill a framebuffer with a 4x4px checkerboard - the same pattern
 *  soynut used on real hardware to validate GDRAM addressing (a wrong
 *  row/column mapping shows up immediately as a skewed or torn grid,
 *  unlike a solid fill which can't reveal addressing bugs at all).
 *  @param fb VGER_LCD_FB_SIZE bytes, zeroed on entry (BSS-initialized
 *            static storage - see vger_firmware_main() below). */
static void vger_firmware_fill_checkerboard(uint8_t *fb)
{
    VGER_ASSERT(fb != NULL);
    VGER_ASSERT(VGER_LCD_WIDTH_PX % 8 == 0); /* byte-packed row assumption below */

    for (int y = 0; y < VGER_LCD_HEIGHT_PX; y++) {
        for (int x = 0; x < VGER_LCD_WIDTH_PX; x++) {
            bool lit = ((x / 4) + (y / 4)) % 2 == 0;
            if (!lit) {
                continue; /* fb starts zeroed; only set the lit bits */
            }
            int byte_idx = y * VGER_LCD_BYTES_PER_ROW + x / 8;
            int bit = 7 - (x % 8);
            fb[byte_idx] = (uint8_t)(fb[byte_idx] | (1u << bit));
        }
    }
}

int main(void)
{
    /* Fixed-size, static storage - Power of 10 rule 3 (no dynamic
     * allocation, ever), same posture as core/vger_state.c's single
     * module-static state instance. Zero-initialized by C, so
     * vger_firmware_fill_checkerboard() can rely on starting from all-off. */
    static uint8_t framebuffer[VGER_LCD_FB_SIZE];

    vger_st7920_init();
    vger_st7920_clear();
    vger_firmware_fill_checkerboard(framebuffer);
    vger_st7920_draw_frame(framebuffer);

    while (true) { /* justified unbounded loop: firmware has no natural
                     * termination besides power-off - the same
                     * documented exception as harness/main.c's event
                     * loop and soynut's own firmware main loops; see
                     * DEVIATIONS.md. */
        tight_loop_contents();
    }
}
