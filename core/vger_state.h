/**
 * @file vger_state.h
 * @brief Public, read-only query API over calculator state.
 *
 * Architecture principle 1: this is the ONLY way any consumer outside
 * core/ (display code, a future menu system, a debugger, the desktop test
 * harness) may observe calculator state. vger_state_t is intentionally
 * opaque here — its fields are defined only in vger_state_internal.h,
 * which nothing outside core/ includes. Extending what's observable means
 * adding a query function here, never poking a new field through.
 */

#ifndef VGER_STATE_H
#define VGER_STATE_H

#include <stdbool.h>
#include <stddef.h>

#include "vger_program.h"
#include "vger_types.h"

/** @brief Opaque calculator state handle. Defined only in
 *  vger_state_internal.h. */
typedef struct vger_state vger_state_t;

/**
 * @brief Get the single calculator state instance.
 *
 * There is exactly one calculator per running program (desktop harness or
 * firmware alike), so state is a module-static instance inside
 * vger_state.c rather than heap-allocated (Power-of-10 rule 3: no dynamic
 * allocation after init — here, none at all). This accessor is the only
 * way to obtain a pointer to it.
 */
vger_state_t *vger_state_get(void);

/** @brief Reset state to its power-on-default values.
 *
 * This is the out-of-band recovery path (architecture principle 5): it is
 * a plain function call, not a key in vger_key_id_t, so it can never be
 * reached through the normal key-dispatch path in vger_interp.c. A host
 * input loop (harness or firmware) wires this to a dedicated control
 * outside its regular key scan, exactly as the DM42 wires its reset to a
 * physical control separate from the keyboard matrix.
 */
void vger_state_reset(vger_state_t *state);

/**
 * @brief Replace the stored program with program and reset the program
 * pointer to its first step. Intended for loading a program parsed by
 * vger_program_parse_text() (e.g. from a file) as an alternative to
 * keystroke programming (PRGM mode); either path ends up here as far as
 * vger_interp_handle_key()'s R/S execution is concerned.
 */
void vger_state_load_program(vger_state_t *state, const vger_program_t *program);

/* ---- Power ---- */
bool vger_get_power_on(const vger_state_t *state);

/* ---- Stack & LASTX ---- */
double vger_get_x(const vger_state_t *state);
double vger_get_y(const vger_state_t *state);
double vger_get_z(const vger_state_t *state);
double vger_get_t(const vger_state_t *state);
double vger_get_lastx(const vger_state_t *state);

/* ---- Storage registers ---- */
vger_register_kind_t vger_get_storage_kind(const vger_state_t *state, int reg);
double vger_get_storage_numeric(const vger_state_t *state, int reg);
/** @brief Copy a register's packed alpha bytes (VGER_ALPHA_PACK_LEN of
 *  them, not NUL-terminated) into out. Only meaningful if
 *  vger_get_storage_kind() == VGER_REG_ALPHA_PACKED. */
void vger_get_storage_alpha(const vger_state_t *state, int reg, char *out);

/* ---- Alpha buffer ---- */
/** @brief Copy the NUL-terminated ALPHA buffer contents into out.
 *  @return The buffer's length (0-24), excluding the NUL. */
size_t vger_get_alpha_buffer(const vger_state_t *state, char *out, size_t out_size);

/* ---- Flags ---- */
bool vger_get_flag(const vger_state_t *state, int flag_num);

/* ---- Display format & annunciators ---- */
vger_angle_mode_t vger_get_angle_mode(const vger_state_t *state);
vger_display_format_t vger_get_display_format(const vger_state_t *state);
int vger_get_display_digits(const vger_state_t *state);
vger_annunciator_state_t vger_get_annunciators(const vger_state_t *state);

/** @brief Render the current display line (X register or ALPHA buffer,
 *  depending on mode) as a NUL-terminated string.
 *  @return Length of the rendered string, excluding the NUL. */
size_t vger_get_display_string(const vger_state_t *state, char *out, size_t out_size);

/* ---- Calculator / program mode ---- */
vger_calc_mode_t vger_get_calc_mode(const vger_state_t *state);
bool vger_get_is_running(const vger_state_t *state);
bool vger_get_is_entering_digits(const vger_state_t *state);
bool vger_get_is_awaiting_argument(const vger_state_t *state);
int vger_get_current_line(const vger_state_t *state);
int vger_get_program_step_count(const vger_state_t *state);

/** @brief Number of pending XEQ subroutine calls (0-VGER_CALL_STACK_MAX_DEPTH). */
int vger_get_call_stack_depth(const vger_state_t *state);

#endif /* VGER_STATE_H */
