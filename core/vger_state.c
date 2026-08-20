/**
 * @file vger_state.c
 * @brief Implementation of the state/query API declared in vger_state.h.
 */

#include "vger_state.h"

#include <stdio.h>
#include <string.h>

#include "vger_assert.h"
#include "vger_program.h"
#include "vger_state_internal.h"

/** @brief The single calculator state instance (rule 3: static, not
 *  heap-allocated; see vger_state_get()'s doc comment for why one
 *  instance is the correct model here). */
static struct vger_state g_state;

vger_state_t *vger_state_get(void)
{
    return &g_state;
}

void vger_state_reset(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);

    memset(state, 0, sizeof(*state));
    state->power_on = true;
    state->stack_lift_enabled = true;
    state->angle_mode = VGER_ANGLE_DEG;
    state->display_format = VGER_DISPLAY_FIX;
    state->display_digits = 4;
    state->calc_mode = VGER_CALC_MODE_RUN;
    state->current_line = -1;
    vger_program_clear(&state->program);

    VGER_ASSERT(state->power_on);
    VGER_ASSERT(state->program.step_count == 0);
}

void vger_state_load_program(vger_state_t *state, const vger_program_t *program)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(program != NULL);

    state->program = *program;
    state->current_line = (program->step_count > 0) ? 0 : -1;

    VGER_ASSERT(state->program.step_count == program->step_count);
}

bool vger_get_power_on(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->power_on;
}

double vger_get_x(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->reg_x;
}

double vger_get_y(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->reg_y;
}

double vger_get_z(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->reg_z;
}

double vger_get_t(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->reg_t;
}

double vger_get_lastx(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->reg_lastx;
}

vger_register_kind_t vger_get_storage_kind(const vger_state_t *state, int reg)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(reg >= 0 && reg < VGER_NUM_STORAGE_REGS);
    return state->storage[reg].kind;
}

double vger_get_storage_numeric(const vger_state_t *state, int reg)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(reg >= 0 && reg < VGER_NUM_STORAGE_REGS);
    return state->storage[reg].numeric;
}

void vger_get_storage_alpha(const vger_state_t *state, int reg, char *out)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out != NULL);
    VGER_ASSERT(reg >= 0 && reg < VGER_NUM_STORAGE_REGS);
    memcpy(out, state->storage[reg].alpha_packed, VGER_ALPHA_PACK_LEN);
}

size_t vger_get_alpha_buffer(const vger_state_t *state, char *out, size_t out_size)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out != NULL && out_size > 0);

    size_t len = (size_t)state->alpha_len;
    if (len + 1U > out_size) {
        len = out_size - 1U;
    }
    memcpy(out, state->alpha_buffer, len);
    out[len] = '\0';
    return len;
}

bool vger_get_flag(const vger_state_t *state, int flag_num)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(flag_num >= 0 && flag_num < VGER_NUM_FLAGS);
    return state->flags[flag_num];
}

vger_angle_mode_t vger_get_angle_mode(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->angle_mode;
}

vger_display_format_t vger_get_display_format(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->display_format;
}

int vger_get_display_digits(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(state->display_digits >= 0 && state->display_digits <= 9);
    return state->display_digits;
}

vger_annunciator_state_t vger_get_annunciators(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);

    vger_annunciator_state_t ann;
    memset(&ann, 0, sizeof(ann));
    ann.alpha_mode = state->alpha_mode;
    ann.user_mode = state->user_mode;
    ann.prgm_mode = (state->calc_mode == VGER_CALC_MODE_PRGM);
    ann.program_running = state->program_running;
    ann.angle_mode = state->angle_mode;
    ann.battery_low = false;
    return ann;
}

/** @brief Format a numeric value per the current display format/digits
 *  into out (bounded by out_size). Returns the formatted length. */
static size_t vger_format_numeric_display(const vger_state_t *state, char *out, size_t out_size)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out != NULL && out_size > 0);

    int written;
    switch (state->display_format) {
        case VGER_DISPLAY_SCI:
            written = snprintf(out, out_size, "%.*e", state->display_digits, state->reg_x);
            break;
        case VGER_DISPLAY_ENG:
            /* Milestone 1 approximates ENG with SCI; true 3-digit-exponent
             * grouping is deferred. */
            written = snprintf(out, out_size, "%.*e", state->display_digits, state->reg_x);
            break;
        case VGER_DISPLAY_FIX:
        default:
            written = snprintf(out, out_size, "%.*f", state->display_digits, state->reg_x);
            break;
    }

    VGER_ASSERT(written >= 0);
    size_t len = (size_t)written;
    return (len < out_size) ? len : (out_size - 1U);
}

size_t vger_get_display_string(const vger_state_t *state, char *out, size_t out_size)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out != NULL && out_size > 0);

    if (!state->power_on) {
        out[0] = '\0';
        return 0;
    }
    if (state->alpha_mode) {
        return vger_get_alpha_buffer(state, out, out_size);
    }
    if (state->entering_digits) {
        size_t len = (size_t)state->digit_entry_len;
        if (len + 1U > out_size) {
            len = out_size - 1U;
        }
        memcpy(out, state->digit_entry_buffer, len);
        out[len] = '\0';
        return len;
    }
    return vger_format_numeric_display(state, out, out_size);
}

vger_calc_mode_t vger_get_calc_mode(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->calc_mode;
}

bool vger_get_is_running(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->program_running;
}

bool vger_get_is_entering_digits(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->entering_digits;
}

bool vger_get_is_awaiting_argument(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->awaiting_argument;
}

int vger_get_current_line(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->current_line;
}

int vger_get_program_step_count(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    return state->program.step_count;
}

int vger_get_call_stack_depth(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(state->call_stack_depth >= 0 && state->call_stack_depth <= VGER_CALL_STACK_MAX_DEPTH);
    return state->call_stack_depth;
}
