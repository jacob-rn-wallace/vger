/**
 * @file vger_state_internal.h
 * @brief Concrete definition of vger_state. Included only by files inside
 * core/ that need to read or write fields directly (vger_state.c and
 * vger_interp.c). Every other consumer — harness, firmware, any future
 * display/menu/debugger code — must go through vger_state.h's query API
 * (architecture principle 1). Do not add a third include site.
 */

#ifndef VGER_STATE_INTERNAL_H
#define VGER_STATE_INTERNAL_H

#include <stdbool.h>

#include "vger_config.h"
#include "vger_program.h"
#include "vger_types.h"

/** @brief Full calculator state. See vger_state.h for the public,
 *  read-only query API; fields here are private to core/. */
struct vger_state {
    bool power_on;

    /* Stack + LASTX. */
    double reg_x;
    double reg_y;
    double reg_z;
    double reg_t;
    double reg_lastx;
    bool stack_lift_enabled; /**< Governs whether the next value placed in
                               *   X pushes the current X into Y first. */

    /* Storage registers 00-99. */
    vger_register_t storage[VGER_NUM_STORAGE_REGS];

    /* Alpha buffer. alpha_len is the authoritative length; alpha_buffer is
     * kept NUL-terminated at alpha_len purely for convenience/debugging. */
    char alpha_buffer[VGER_ALPHA_BUFFER_LEN + 1];
    int alpha_len;

    /* Flags 00-99 (only 00-29 are user-settable in milestone 1). */
    bool flags[VGER_NUM_FLAGS];

    vger_angle_mode_t angle_mode;
    vger_display_format_t display_format;
    int display_digits; /**< 0-9, meaning depends on display_format. */

    vger_calc_mode_t calc_mode; /**< RUN or PRGM. */
    bool alpha_mode; /**< ALPHA data-entry mode active. */
    bool user_mode; /**< USER key toggle; stub, no key-remap behavior yet. */

    /* Digit-entry sub-state: true while the user is mid-way through typing
     * a number (between the first digit and a non-digit key). Needed both
     * to know whether ENTER should push the stack and to answer "is idle"
     * for the mode-boundary policy. */
    bool entering_digits;
    char digit_entry_buffer[24];
    int digit_entry_len;

    /* Pending-argument sub-state: true after a key like STO/RCL/GTO/SF is
     * pressed in RUN mode, while waiting for the following one or two
     * digit keys that supply its numeric argument. */
    bool awaiting_argument;
    vger_key_id_t awaiting_argument_for;
    char argument_digits[3];
    int argument_digit_count;

    /* Program storage and execution. */
    vger_program_t program;
    int current_line; /**< Index into program.steps; -1 if undefined. */
    bool program_running;
    long steps_executed_this_run;
};

#endif /* VGER_STATE_INTERNAL_H */
