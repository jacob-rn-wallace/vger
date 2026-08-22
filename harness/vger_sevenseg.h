/**
 * @file vger_sevenseg.h
 * @brief Minimal seven-segment digit renderer for the desktop test harness.
 *
 * Deliberately not a general text/font renderer: milestone 1 only needs
 * to prove the display-string query drives *something* pixel-based, in a
 * coordinate space that anticipates the real 144x32 NHD14432/ST7920
 * target. A bitmap alphabet font is out of scope here; the full ALPHA
 * buffer and register/flag state are dumped to the terminal instead (see
 * main.c), which exercises the query API just as thoroughly without a
 * hand-built glyph table.
 */

#ifndef VGER_SEVENSEG_H
#define VGER_SEVENSEG_H

#include <SDL2/SDL.h>

/** @brief Draw one character (digit, '.', '-', or space) as a seven-segment
 *  glyph at (x, y) with the given cell size, using SDL_RenderFillRect for
 *  lit segments. Unrecognized characters render as blank.
 *
 * @param renderer Target renderer; caller owns it.
 * @param ch Character to render.
 * @param x Left edge of the glyph cell, in renderer (logical) pixels.
 * @param y Top edge of the glyph cell.
 * @param w Cell width.
 * @param h Cell height.
 */
void vger_sevenseg_draw_char(SDL_Renderer *renderer, char ch, int x, int y, int w, int h);

/**
 * @brief Draw a NUL-terminated string as a row of seven-segment glyphs.
 *
 * @param renderer Target renderer.
 * @param text String to render; only the first max_chars are drawn.
 * @param x Left edge of the first glyph cell.
 * @param y Top edge of the row.
 * @param cell_w Width of each glyph cell.
 * @param cell_h Height of each glyph cell.
 * @param max_chars Bound on how many characters are drawn (rule 2: every
 *                   loop needs a fixed upper bound).
 */
void vger_sevenseg_draw_string(SDL_Renderer *renderer, const char *text, int x, int y, int cell_w, int cell_h, int max_chars);

#endif /* VGER_SEVENSEG_H */
