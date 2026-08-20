/**
 * @file vger_sevenseg.c
 * @brief Implementation of the seven-segment glyph renderer.
 */

#include "vger_sevenseg.h"

/* Segment bits, standard seven-segment layout:
 *    _a_
 *   f   b
 *    _g_
 *   e   c
 *    _d_
 */
#define SEG_A 0x01
#define SEG_B 0x02
#define SEG_C 0x04
#define SEG_D 0x08
#define SEG_E 0x10
#define SEG_F 0x20
#define SEG_G 0x40

/** @brief Look up the segment mask for one character.
 *  @return Segment bitmask, or 0 (blank) for any character with no glyph. */
static unsigned char vger_sevenseg_lookup(char ch)
{
    switch (ch) {
        case '0':
            return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
        case '1':
            return SEG_B | SEG_C;
        case '2':
            return SEG_A | SEG_B | SEG_G | SEG_E | SEG_D;
        case '3':
            return SEG_A | SEG_B | SEG_G | SEG_C | SEG_D;
        case '4':
            return SEG_F | SEG_G | SEG_B | SEG_C;
        case '5':
            return SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
        case '6':
            return SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D;
        case '7':
            return SEG_A | SEG_B | SEG_C;
        case '8':
            return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case '9':
            return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
        case '-':
            return SEG_G;
        default:
            return 0; /* space, '.', and anything else: blank cell */
    }
}

void vger_sevenseg_draw_char(SDL_Renderer *renderer, char ch, int x, int y, int w, int h)
{
    if (renderer == NULL || w <= 0 || h <= 0) {
        return;
    }

    if (ch == '.') {
        SDL_Rect dot = {x + w - (w / 6), y + h - (h / 6), w / 6, h / 6};
        (void)SDL_RenderFillRect(renderer, &dot);
        return;
    }

    unsigned char mask = vger_sevenseg_lookup(ch);
    int thickness = (w < h ? w : h) / 6;
    if (thickness < 2) {
        thickness = 2;
    }
    int half_h = h / 2;

    SDL_Rect seg_a = {x + thickness, y, w - (2 * thickness), thickness};
    SDL_Rect seg_g = {x + thickness, y + half_h - (thickness / 2), w - (2 * thickness), thickness};
    SDL_Rect seg_d = {x + thickness, y + h - thickness, w - (2 * thickness), thickness};
    SDL_Rect seg_f = {x, y, thickness, half_h};
    SDL_Rect seg_b = {x + w - thickness, y, thickness, half_h};
    SDL_Rect seg_e = {x, y + half_h, thickness, half_h};
    SDL_Rect seg_c = {x + w - thickness, y + half_h, thickness, half_h};

    if (mask & SEG_A) {
        (void)SDL_RenderFillRect(renderer, &seg_a);
    }
    if (mask & SEG_B) {
        (void)SDL_RenderFillRect(renderer, &seg_b);
    }
    if (mask & SEG_C) {
        (void)SDL_RenderFillRect(renderer, &seg_c);
    }
    if (mask & SEG_D) {
        (void)SDL_RenderFillRect(renderer, &seg_d);
    }
    if (mask & SEG_E) {
        (void)SDL_RenderFillRect(renderer, &seg_e);
    }
    if (mask & SEG_F) {
        (void)SDL_RenderFillRect(renderer, &seg_f);
    }
    if (mask & SEG_G) {
        (void)SDL_RenderFillRect(renderer, &seg_g);
    }
}

void vger_sevenseg_draw_string(SDL_Renderer *renderer, const char *text, int x, int y, int cell_w, int cell_h, int max_chars)
{
    if (renderer == NULL || text == NULL || max_chars <= 0) {
        return;
    }

    int cursor_x = x;
    for (int i = 0; i < max_chars && text[i] != '\0'; i++) {
        if (text[i] == '.') {
            vger_sevenseg_draw_char(renderer, '.', cursor_x - (cell_w / 3), y, cell_w, cell_h);
            continue; /* decimal point rides on the previous cell, no advance */
        }
        vger_sevenseg_draw_char(renderer, text[i], cursor_x, y, cell_w, cell_h);
        cursor_x += cell_w + (cell_w / 6);
    }
}
