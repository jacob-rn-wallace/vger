/**
 * @file vger_types.h
 * @brief Shared value types used across the vger FOCAL core's public API.
 */

#ifndef VGER_TYPES_H
#define VGER_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#include "vger_config.h"

/** @brief What kind of value a storage register currently holds. */
typedef enum {
    VGER_REG_NUMERIC = 0, /**< Register holds a numeric (double) value. */
    VGER_REG_ALPHA_PACKED = 1 /**< Register holds 6 packed alpha characters (ASTO). */
} vger_register_kind_t;

/** @brief Content of one storage register (tagged union, no dynamic parts). */
typedef struct {
    vger_register_kind_t kind;
    double numeric;
    char alpha_packed[VGER_ALPHA_PACK_LEN]; /**< Valid iff kind == VGER_REG_ALPHA_PACKED. Not NUL-terminated. */
} vger_register_t;

/** @brief Calculator angle mode, shown via an annunciator-equivalent query. */
typedef enum {
    VGER_ANGLE_DEG = 0,
    VGER_ANGLE_RAD = 1,
    VGER_ANGLE_GRAD = 2
} vger_angle_mode_t;

/** @brief Display number format, set by FIX/SCI/ENG. */
typedef enum {
    VGER_DISPLAY_FIX = 0,
    VGER_DISPLAY_SCI = 1,
    VGER_DISPLAY_ENG = 2
} vger_display_format_t;

/** @brief Run/program-entry mode, toggled by the PRGM key. */
typedef enum {
    VGER_CALC_MODE_RUN = 0,
    VGER_CALC_MODE_PRGM = 1
} vger_calc_mode_t;

/**
 * @brief Annunciator states, derived from other state rather than stored
 * redundantly. This is a first-class query result (architecture principle
 * 1), computed fresh on every call from the fields it depends on.
 */
typedef struct {
    bool alpha_mode;
    bool user_mode;
    bool prgm_mode;
    bool program_running;
    vger_angle_mode_t angle_mode;
    bool battery_low; /**< Always false in the desktop/firmware prototype. */
} vger_annunciator_state_t;

/**
 * @brief Logical key identifiers the core's key-dispatch API accepts.
 *
 * This is the core's documented-FOCAL key vocabulary for milestone 1, not
 * a physical keyboard scan code. A harness or firmware input layer maps
 * whatever raw input it receives (SDL2 key events, a keypad matrix scan)
 * onto these values before calling vger_interp_handle_key().
 *
 * Deliberately excluded here: MENU. MENU belongs to the extended/system
 * menu layer (architecture principle 2) and is never passed into the core
 * interpreter; a host input loop intercepts it before reaching this API.
 */
typedef enum {
    VGER_KEY_ON = 0,
    VGER_KEY_USER,
    VGER_KEY_DIGIT_0,
    VGER_KEY_DIGIT_1,
    VGER_KEY_DIGIT_2,
    VGER_KEY_DIGIT_3,
    VGER_KEY_DIGIT_4,
    VGER_KEY_DIGIT_5,
    VGER_KEY_DIGIT_6,
    VGER_KEY_DIGIT_7,
    VGER_KEY_DIGIT_8,
    VGER_KEY_DIGIT_9,
    VGER_KEY_DECIMAL_POINT,
    VGER_KEY_CHS,
    VGER_KEY_ENTER,
    VGER_KEY_PLUS,
    VGER_KEY_MINUS,
    VGER_KEY_TIMES,
    VGER_KEY_DIVIDE,
    VGER_KEY_X_EXCHANGE_Y,
    VGER_KEY_CLX,
    VGER_KEY_LASTX,
    VGER_KEY_STO,
    VGER_KEY_RCL,
    VGER_KEY_IND, /**< Indirect-addressing prefix: press right after an
                    *  argument-taking opcode (except LBL) so the following
                    *  2 digits name a pointer register instead of the
                    *  target directly. */
    VGER_KEY_GTO,
    VGER_KEY_LBL,
    VGER_KEY_XEQ,
    VGER_KEY_RTN,
    VGER_KEY_END,
    VGER_KEY_RUN_STOP,
    VGER_KEY_SF,
    VGER_KEY_CF,
    VGER_KEY_FS_QUESTION_C,
    /* Conditional-skip test instructions: non-destructive, skip the next
     * program step iff the named condition is false (FS?C's polarity). */
    VGER_KEY_X_EQ_0,
    VGER_KEY_X_NE_0,
    VGER_KEY_X_GT_0,
    VGER_KEY_X_LT_0,
    VGER_KEY_X_EQ_Y,
    VGER_KEY_X_NE_Y,
    VGER_KEY_X_GT_Y,
    VGER_KEY_X_LT_Y,
    VGER_KEY_ALPHA,
    VGER_KEY_ASTO,
    VGER_KEY_ARCL,
    VGER_KEY_FIX,
    VGER_KEY_SCI,
    VGER_KEY_ENG,
    VGER_KEY_PRGM,
    VGER_KEY_ALPHA_CHAR, /**< Payload key: append ch to the ALPHA buffer. */
    VGER_KEY_BACKSPACE,
    VGER_KEY_COUNT
} vger_key_id_t;

/** @brief One key-press event passed into the interpreter.
 *
 * ch is only meaningful when id == VGER_KEY_ALPHA_CHAR (the harness sends
 * one event per typed character while ALPHA mode is active, rather than
 * the core owning a full alpha keyboard shift table).
 */
typedef struct {
    vger_key_id_t id;
    char ch;
} vger_key_event_t;

#endif /* VGER_TYPES_H */
