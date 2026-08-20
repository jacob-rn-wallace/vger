/**
 * @file vger_keymap.c
 * @brief Implementation of the host scancode -> core key classifier.
 */

#include "vger_keymap.h"

#include <string.h>

/** @brief One function-key binding: a scancode and the core key it means. */
typedef struct {
    SDL_Scancode scancode;
    vger_key_id_t key;
} vger_binding_t;

static const vger_binding_t VGER_BINDINGS[] = {
    {SDL_SCANCODE_RETURN, VGER_KEY_ENTER},
    {SDL_SCANCODE_KP_ENTER, VGER_KEY_ENTER},
    {SDL_SCANCODE_BACKSPACE, VGER_KEY_BACKSPACE},
    {SDL_SCANCODE_KP_PLUS, VGER_KEY_PLUS},
    {SDL_SCANCODE_KP_MINUS, VGER_KEY_MINUS},
    {SDL_SCANCODE_KP_MULTIPLY, VGER_KEY_TIMES},
    {SDL_SCANCODE_KP_DIVIDE, VGER_KEY_DIVIDE},
    {SDL_SCANCODE_TAB, VGER_KEY_X_EXCHANGE_Y},
    {SDL_SCANCODE_DELETE, VGER_KEY_CLX},
    {SDL_SCANCODE_F1, VGER_KEY_STO},
    {SDL_SCANCODE_F2, VGER_KEY_RCL},
    {SDL_SCANCODE_I, VGER_KEY_IND},
    {SDL_SCANCODE_F3, VGER_KEY_GTO},
    {SDL_SCANCODE_F4, VGER_KEY_LBL},
    {SDL_SCANCODE_X, VGER_KEY_XEQ},
    {SDL_SCANCODE_R, VGER_KEY_RTN},
    {SDL_SCANCODE_F5, VGER_KEY_SF},
    {SDL_SCANCODE_F6, VGER_KEY_CF},
    {SDL_SCANCODE_F7, VGER_KEY_FS_QUESTION_C},
    {SDL_SCANCODE_F8, VGER_KEY_ASTO},
    {SDL_SCANCODE_F9, VGER_KEY_ARCL},
    {SDL_SCANCODE_F10, VGER_KEY_FIX},
    {SDL_SCANCODE_COMMA, VGER_KEY_SCI},
    {SDL_SCANCODE_SEMICOLON, VGER_KEY_ENG},
    {SDL_SCANCODE_N, VGER_KEY_CHS},
    {SDL_SCANCODE_L, VGER_KEY_LASTX},
    {SDL_SCANCODE_SPACE, VGER_KEY_RUN_STOP},
    {SDL_SCANCODE_P, VGER_KEY_PRGM},
    {SDL_SCANCODE_U, VGER_KEY_USER},
    {SDL_SCANCODE_HOME, VGER_KEY_ON},
    {SDL_SCANCODE_PERIOD, VGER_KEY_DECIMAL_POINT},
    {SDL_SCANCODE_0, VGER_KEY_DIGIT_0},
    {SDL_SCANCODE_1, VGER_KEY_DIGIT_1},
    {SDL_SCANCODE_2, VGER_KEY_DIGIT_2},
    {SDL_SCANCODE_3, VGER_KEY_DIGIT_3},
    {SDL_SCANCODE_4, VGER_KEY_DIGIT_4},
    {SDL_SCANCODE_5, VGER_KEY_DIGIT_5},
    {SDL_SCANCODE_6, VGER_KEY_DIGIT_6},
    {SDL_SCANCODE_7, VGER_KEY_DIGIT_7},
    {SDL_SCANCODE_8, VGER_KEY_DIGIT_8},
    {SDL_SCANCODE_9, VGER_KEY_DIGIT_9},
    {SDL_SCANCODE_KP_0, VGER_KEY_DIGIT_0},
    {SDL_SCANCODE_KP_1, VGER_KEY_DIGIT_1},
    {SDL_SCANCODE_KP_2, VGER_KEY_DIGIT_2},
    {SDL_SCANCODE_KP_3, VGER_KEY_DIGIT_3},
    {SDL_SCANCODE_KP_4, VGER_KEY_DIGIT_4},
    {SDL_SCANCODE_KP_5, VGER_KEY_DIGIT_5},
    {SDL_SCANCODE_KP_6, VGER_KEY_DIGIT_6},
    {SDL_SCANCODE_KP_7, VGER_KEY_DIGIT_7},
    {SDL_SCANCODE_KP_8, VGER_KEY_DIGIT_8},
    {SDL_SCANCODE_KP_9, VGER_KEY_DIGIT_9},
};
#define VGER_BINDING_COUNT (sizeof(VGER_BINDINGS) / sizeof(VGER_BINDINGS[0]))

/** @brief If scancode is a printable A-Z/0-9/space key, fill *out_ch with
 *  its character and return true. */
static bool vger_scancode_to_printable(SDL_Scancode scancode, char *out_ch)
{
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        *out_ch = (char)('A' + (scancode - SDL_SCANCODE_A));
        return true;
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        *out_ch = (char)('1' + (scancode - SDL_SCANCODE_1));
        return true;
    }
    if (scancode == SDL_SCANCODE_0) {
        *out_ch = '0';
        return true;
    }
    if (scancode == SDL_SCANCODE_SPACE) {
        *out_ch = ' ';
        return true;
    }
    if (scancode == SDL_SCANCODE_PERIOD) {
        *out_ch = '.';
        return true;
    }
    return false;
}

vger_host_action_t vger_keymap_classify(SDL_Scancode scancode, bool alpha_mode_active, vger_key_event_t *out_event)
{
    char printable = '\0';
    if (alpha_mode_active && scancode != SDL_SCANCODE_ESCAPE && vger_scancode_to_printable(scancode, &printable)) {
        out_event->id = VGER_KEY_ALPHA_CHAR;
        out_event->ch = printable;
        return VGER_HOST_ACTION_CORE_KEY;
    }

    if (scancode == SDL_SCANCODE_ESCAPE) {
        out_event->id = VGER_KEY_ALPHA;
        out_event->ch = '\0';
        return VGER_HOST_ACTION_CORE_KEY;
    }
    if (scancode == SDL_SCANCODE_INSERT) {
        return VGER_HOST_ACTION_MENU;
    }
    if (scancode == SDL_SCANCODE_F12) {
        return VGER_HOST_ACTION_RESET;
    }

    for (size_t i = 0; i < VGER_BINDING_COUNT; i++) {
        if (VGER_BINDINGS[i].scancode == scancode) {
            out_event->id = VGER_BINDINGS[i].key;
            out_event->ch = '\0';
            return VGER_HOST_ACTION_CORE_KEY;
        }
    }

    return VGER_HOST_ACTION_NONE;
}
