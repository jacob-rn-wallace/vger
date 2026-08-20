/**
 * @file vger_interp.c
 * @brief FOCAL key dispatch, digit/argument entry, and program execution.
 *
 * Milestone-1 scope notes (see README.md for the full list):
 *  - PRGM-mode recording is append-only; there is no mid-program insertion
 *    point, and GTO always means "jump" (there is no separate GTO-as-
 *    navigation form).
 *  - R/S runs the program synchronously to completion within a single
 *    vger_interp_handle_key() call; there is no live single-step/paused
 *    "running" state observable between keystrokes.
 *  - A non-digit key while an argument (STO nn, GTO nn, ...) is pending
 *    cancels the pending instruction; the interrupting key itself is
 *    dropped rather than reprocessed, to avoid recursion (rule 1) at the
 *    top of the dispatcher.
 */

#include "vger_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vger_assert.h"
#include "vger_program.h"
#include "vger_state_internal.h"

/** @brief Describes the argument shape a key that starts an
 *  argument-taking instruction expects. */
typedef struct {
    vger_key_id_t key;
    vger_step_kind_t kind;
    int digit_count; /**< How many digit keys complete the argument. */
} vger_arg_spec_entry_t;

static const vger_arg_spec_entry_t VGER_ARG_SPECS[] = {
    {VGER_KEY_STO, VGER_STEP_OPCODE_REG_ARG, 2},
    {VGER_KEY_RCL, VGER_STEP_OPCODE_REG_ARG, 2},
    {VGER_KEY_ASTO, VGER_STEP_OPCODE_REG_ARG, 2},
    {VGER_KEY_ARCL, VGER_STEP_OPCODE_REG_ARG, 2},
    {VGER_KEY_SF, VGER_STEP_OPCODE_FLAG_ARG, 2},
    {VGER_KEY_CF, VGER_STEP_OPCODE_FLAG_ARG, 2},
    {VGER_KEY_FS_QUESTION_C, VGER_STEP_OPCODE_FLAG_ARG, 2},
    {VGER_KEY_LBL, VGER_STEP_OPCODE_LABEL_ARG, 2},
    {VGER_KEY_GTO, VGER_STEP_OPCODE_LABEL_ARG, 2},
    {VGER_KEY_XEQ, VGER_STEP_OPCODE_LABEL_ARG, 2},
    {VGER_KEY_FIX, VGER_STEP_OPCODE_DIGITS_ARG, 1},
    {VGER_KEY_SCI, VGER_STEP_OPCODE_DIGITS_ARG, 1},
    {VGER_KEY_ENG, VGER_STEP_OPCODE_DIGITS_ARG, 1},
};
#define VGER_ARG_SPEC_COUNT (sizeof(VGER_ARG_SPECS) / sizeof(VGER_ARG_SPECS[0]))

/** @brief Result of executing one instruction; tells the caller (a direct
 *  keypress commit, or the R/S run loop) what control-flow to apply. */
typedef struct {
    bool jumped; /**< io_current_line was set directly (GTO/XEQ/RTN); don't also increment it. */
    bool skip_next; /**< The following program step should be skipped. */
    bool halt; /**< Execution should stop (END, R/S, empty-stack RTN, or call-stack overflow). */
} vger_exec_result_t;

static vger_exec_result_t vger_execute_step(vger_state_t *state, const vger_program_step_t *step, int *io_current_line);

/* ---------------------------------------------------------------------- */
/* Stack helpers                                                          */
/* ---------------------------------------------------------------------- */

/** @brief Push X into Y/Z/T (dropping T), unconditionally. */
static void vger_stack_push_unconditional(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    state->reg_t = state->reg_z;
    state->reg_z = state->reg_y;
    state->reg_y = state->reg_x;
    VGER_ASSERT(state != NULL);
}

/** @brief Push X into Y/Z/T (dropping T) iff stack lift is enabled. Used
 *  by operations that place a *new* value into X (digit-entry start,
 *  RCL, LASTX) — as opposed to ENTER, which always pushes regardless of
 *  the flag and is implemented with vger_stack_push_unconditional()
 *  instead. */
static void vger_stack_push_lift(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    if (state->stack_lift_enabled) {
        vger_stack_push_unconditional(state);
    }
    VGER_ASSERT(state->stack_lift_enabled == true || state->stack_lift_enabled == false);
}

/** @brief Drop the stack after a dyadic operation: Y->Y? no - Z->Y, T->Z,
 *  T duplicates itself (real HP-41 "stack drop" behavior). */
static void vger_stack_drop(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    state->reg_y = state->reg_z;
    state->reg_z = state->reg_t;
    VGER_ASSERT(state != NULL);
}

/* ---------------------------------------------------------------------- */
/* Digit / alpha entry                                                    */
/* ---------------------------------------------------------------------- */

/** @brief Commit the in-progress digit-entry buffer and clear the entry
 *  sub-state. No-op if not currently entering digits. In RUN mode X was
 *  already kept live by vger_handle_digit_entry_key, so this just closes
 *  the entry out; in PRGM mode the accumulated number is recorded as a
 *  NUMBER_LITERAL program step (digit entry never touches the live
 *  registers while recording). */
static void vger_flush_digit_entry(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    if (!state->entering_digits) {
        return;
    }

    char *end = NULL;
    double value = strtod(state->digit_entry_buffer, &end);
    VGER_ASSERT(end != NULL);

    if (state->calc_mode == VGER_CALC_MODE_PRGM) {
        if (state->program.step_count < VGER_PROGRAM_MAX_LINES) {
            vger_program_step_t step;
            memset(&step, 0, sizeof(step));
            step.kind = VGER_STEP_NUMBER_LITERAL;
            step.number_value = value;
            state->program.steps[state->program.step_count] = step;
            state->program.step_count++;
        }
    } else {
        state->reg_x = value;
    }

    state->entering_digits = false;
    state->digit_entry_len = 0;
    state->digit_entry_buffer[0] = '\0';
    VGER_ASSERT(!state->entering_digits);
}

/** @brief Handle a digit or decimal-point key: start or continue building
 *  the digit-entry buffer. In RUN mode this also keeps X live (honoring
 *  stack lift on the first digit of a fresh entry, matching real HP-41
 *  behavior where X updates as you type); in PRGM mode the buffer is
 *  display-only bookkeeping until vger_flush_digit_entry() records it as a
 *  step, so it never touches the stack. */
static void vger_handle_digit_entry_key(vger_state_t *state, vger_key_id_t key)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(key == VGER_KEY_DECIMAL_POINT || (key >= VGER_KEY_DIGIT_0 && key <= VGER_KEY_DIGIT_9));

    bool is_run_mode = (state->calc_mode == VGER_CALC_MODE_RUN);

    if (!state->entering_digits) {
        if (is_run_mode) {
            vger_stack_push_lift(state);
        }
        state->entering_digits = true;
        state->digit_entry_len = 0;
        state->digit_entry_buffer[0] = '\0';
    }

    if (key == VGER_KEY_DECIMAL_POINT) {
        for (int i = 0; i < state->digit_entry_len; i++) {
            if (state->digit_entry_buffer[i] == '.') {
                return; /* only one decimal point allowed */
            }
        }
    }

    if ((size_t)state->digit_entry_len + 1U >= sizeof(state->digit_entry_buffer)) {
        return; /* entry buffer full; drop further digits */
    }

    char ch = (key == VGER_KEY_DECIMAL_POINT) ? '.' : (char)('0' + (key - VGER_KEY_DIGIT_0));
    state->digit_entry_buffer[state->digit_entry_len] = ch;
    state->digit_entry_len++;
    state->digit_entry_buffer[state->digit_entry_len] = '\0';

    if (is_run_mode) {
        state->reg_x = strtod(state->digit_entry_buffer, NULL);
    }

    VGER_ASSERT(state->digit_entry_len >= 0);
    VGER_ASSERT((size_t)state->digit_entry_len < sizeof(state->digit_entry_buffer));
}

/** @brief Toggle the sign of the in-progress digit-entry buffer. */
static void vger_toggle_entry_sign(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(state->entering_digits);

    if (state->digit_entry_buffer[0] == '-') {
        memmove(state->digit_entry_buffer, state->digit_entry_buffer + 1, (size_t)state->digit_entry_len);
        state->digit_entry_len--;
    } else if ((size_t)state->digit_entry_len + 1U < sizeof(state->digit_entry_buffer)) {
        memmove(state->digit_entry_buffer + 1, state->digit_entry_buffer, (size_t)state->digit_entry_len + 1U);
        state->digit_entry_buffer[0] = '-';
        state->digit_entry_len++;
    }
    VGER_ASSERT(state->digit_entry_len >= 0);
}

/** @brief Toggle ALPHA mode. Closing PRGM-mode alpha entry commits the
 *  composed text as an ALPHA_LITERAL program step. */
static void vger_handle_alpha_toggle(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    bool was_on = state->alpha_mode;
    state->alpha_mode = !state->alpha_mode;

    if (was_on && state->calc_mode == VGER_CALC_MODE_PRGM && state->program.step_count < VGER_PROGRAM_MAX_LINES) {
        vger_program_step_t step;
        memset(&step, 0, sizeof(step));
        step.kind = VGER_STEP_ALPHA_LITERAL;
        (void)vger_get_alpha_buffer(state, step.alpha_value, sizeof(step.alpha_value));
        state->program.steps[state->program.step_count] = step;
        state->program.step_count++;
    }
    VGER_ASSERT(state->alpha_mode == true || state->alpha_mode == false);
}

/** @brief Append one character to the ALPHA buffer (bounded, truncates
 *  silently past VGER_ALPHA_BUFFER_LEN, matching the real 24-char cap). */
static void vger_handle_alpha_char(vger_state_t *state, char ch)
{
    VGER_ASSERT(state != NULL);
    if (state->alpha_len < VGER_ALPHA_BUFFER_LEN) {
        state->alpha_buffer[state->alpha_len] = ch;
        state->alpha_len++;
        state->alpha_buffer[state->alpha_len] = '\0';
    }
    VGER_ASSERT(state->alpha_len <= VGER_ALPHA_BUFFER_LEN);
}

/** @brief Backspace: erase the last character of whichever entry is
 *  active (digit entry, alpha entry), preferring digit entry. */
static void vger_handle_backspace(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    if (state->entering_digits && state->digit_entry_len > 0) {
        state->digit_entry_len--;
        state->digit_entry_buffer[state->digit_entry_len] = '\0';
    } else if (state->alpha_mode && state->alpha_len > 0) {
        state->alpha_len--;
        state->alpha_buffer[state->alpha_len] = '\0';
    }
    VGER_ASSERT(state->digit_entry_len >= 0);
    VGER_ASSERT(state->alpha_len >= 0);
}

/* ---------------------------------------------------------------------- */
/* Call stack helpers (XEQ/RTN)                                           */
/* ---------------------------------------------------------------------- */

/** @brief Push return_line onto the subroutine call stack.
 *  @return false if the stack is already at VGER_CALL_STACK_MAX_DEPTH
 *          (caller halts rather than corrupting state - see
 *          VGER_CALL_STACK_MAX_DEPTH's doc comment). */
static bool vger_call_stack_push(vger_state_t *state, int return_line)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(state->call_stack_depth >= 0);
    if (state->call_stack_depth >= VGER_CALL_STACK_MAX_DEPTH) {
        return false;
    }
    state->call_stack[state->call_stack_depth] = return_line;
    state->call_stack_depth++;
    VGER_ASSERT(state->call_stack_depth <= VGER_CALL_STACK_MAX_DEPTH);
    return true;
}

/** @brief Pop the most recent return address into *out_line.
 *  @return false if the stack is empty (caller treats RTN like END). */
static bool vger_call_stack_pop(vger_state_t *state, int *out_line)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out_line != NULL);
    if (state->call_stack_depth <= 0) {
        return false;
    }
    state->call_stack_depth--;
    *out_line = state->call_stack[state->call_stack_depth];
    VGER_ASSERT(state->call_stack_depth >= 0);
    return true;
}

/* ---------------------------------------------------------------------- */
/* Instruction commit (RUN: execute now; PRGM: record as a step)          */
/* ---------------------------------------------------------------------- */

static bool vger_lookup_arg_spec(vger_key_id_t key, vger_arg_spec_entry_t *out)
{
    VGER_ASSERT(out != NULL);
    for (size_t i = 0; i < VGER_ARG_SPEC_COUNT; i++) {
        if (VGER_ARG_SPECS[i].key == key) {
            *out = VGER_ARG_SPECS[i];
            return true;
        }
    }
    return false;
}

/** @brief Commit a fully-formed instruction: record it if in PRGM mode,
 *  execute it immediately if in RUN mode. */
static void vger_commit_instruction(vger_state_t *state, vger_step_kind_t kind, vger_key_id_t opcode, int int_arg)
{
    VGER_ASSERT(state != NULL);

    vger_program_step_t step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    step.opcode = opcode;
    step.int_arg = int_arg;

    if (state->calc_mode == VGER_CALC_MODE_PRGM) {
        if (state->program.step_count < VGER_PROGRAM_MAX_LINES) {
            state->program.steps[state->program.step_count] = step;
            state->program.step_count++;
        }
        return;
    }

    int line = state->current_line;
    vger_exec_result_t result = vger_execute_step(state, &step, &line);
    if (result.jumped) {
        state->current_line = line;
    }
    VGER_ASSERT(state->calc_mode == VGER_CALC_MODE_RUN);
}

/** @brief Handle a digit key while an argument (STO nn, GTO nn, ...) is
 *  pending. A non-digit key cancels the pending instruction instead. */
static void vger_handle_argument_digit_or_abort(vger_state_t *state, vger_key_event_t event)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(state->awaiting_argument);

    if (event.id < VGER_KEY_DIGIT_0 || event.id > VGER_KEY_DIGIT_9) {
        state->awaiting_argument = false;
        state->argument_digit_count = 0;
        return;
    }

    vger_arg_spec_entry_t spec;
    bool found = vger_lookup_arg_spec(state->awaiting_argument_for, &spec);
    VGER_ASSERT(found);

    char digit_ch = (char)('0' + (event.id - VGER_KEY_DIGIT_0));
    state->argument_digits[state->argument_digit_count] = digit_ch;
    state->argument_digit_count++;

    if (state->argument_digit_count < spec.digit_count) {
        return;
    }

    state->argument_digits[state->argument_digit_count] = '\0';
    int arg_value = (int)strtol(state->argument_digits, NULL, 10);
    vger_key_id_t opcode = state->awaiting_argument_for;

    state->awaiting_argument = false;
    state->argument_digit_count = 0;
    vger_commit_instruction(state, spec.kind, opcode, arg_value);
}

/* ---------------------------------------------------------------------- */
/* Execution: shared by immediate keypress commit and the R/S run loop    */
/* ---------------------------------------------------------------------- */

static void vger_exec_opcode_only(vger_state_t *state, vger_key_id_t opcode, int *io_current_line, vger_exec_result_t *out)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(io_current_line != NULL);
    VGER_ASSERT(out != NULL);

    switch (opcode) {
        case VGER_KEY_ENTER:
            vger_stack_push_unconditional(state);
            state->stack_lift_enabled = false;
            break;
        case VGER_KEY_CLX:
            state->reg_lastx = state->reg_x;
            state->reg_x = 0.0;
            state->stack_lift_enabled = false;
            break;
        case VGER_KEY_CHS:
            state->reg_x = -state->reg_x;
            break;
        case VGER_KEY_X_EXCHANGE_Y: {
            double tmp = state->reg_x;
            state->reg_x = state->reg_y;
            state->reg_y = tmp;
            break;
        }
        case VGER_KEY_LASTX:
            vger_stack_push_lift(state);
            state->reg_x = state->reg_lastx;
            state->stack_lift_enabled = true;
            break;
        case VGER_KEY_PLUS:
        case VGER_KEY_MINUS:
        case VGER_KEY_TIMES:
        case VGER_KEY_DIVIDE: {
            state->reg_lastx = state->reg_x;
            double y = state->reg_y;
            double x = state->reg_x;
            state->reg_x = (opcode == VGER_KEY_PLUS)    ? y + x
                            : (opcode == VGER_KEY_MINUS) ? y - x
                            : (opcode == VGER_KEY_TIMES) ? y * x
                                                          : ((x != 0.0) ? y / x : 0.0);
            vger_stack_drop(state);
            state->stack_lift_enabled = true;
            break;
        }
        case VGER_KEY_RTN: {
            int return_line;
            if (vger_call_stack_pop(state, &return_line)) {
                *io_current_line = return_line;
                out->jumped = true;
            } else {
                out->halt = true; /* RTN with nothing to return to: same as END */
            }
            break;
        }
        case VGER_KEY_END:
        case VGER_KEY_RUN_STOP:
            out->halt = true;
            break;
        case VGER_KEY_LBL:
        default:
            break; /* no-op as a bare opcode */
    }
}

static void vger_exec_reg_arg(vger_state_t *state, vger_key_id_t opcode, int reg)
{
    VGER_ASSERT(state != NULL);
    if (reg < 0 || reg >= VGER_NUM_STORAGE_REGS) {
        return;
    }

    switch (opcode) {
        case VGER_KEY_STO:
            state->storage[reg].kind = VGER_REG_NUMERIC;
            state->storage[reg].numeric = state->reg_x;
            break;
        case VGER_KEY_RCL:
            vger_stack_push_lift(state);
            state->reg_x = (state->storage[reg].kind == VGER_REG_NUMERIC) ? state->storage[reg].numeric : 0.0;
            state->stack_lift_enabled = true;
            break;
        case VGER_KEY_ASTO:
            state->storage[reg].kind = VGER_REG_ALPHA_PACKED;
            memset(state->storage[reg].alpha_packed, ' ', VGER_ALPHA_PACK_LEN);
            memcpy(state->storage[reg].alpha_packed, state->alpha_buffer,
                   (size_t)(state->alpha_len < VGER_ALPHA_PACK_LEN ? state->alpha_len : VGER_ALPHA_PACK_LEN));
            break;
        case VGER_KEY_ARCL:
            if (state->storage[reg].kind == VGER_REG_ALPHA_PACKED) {
                for (int i = 0; i < VGER_ALPHA_PACK_LEN; i++) {
                    vger_handle_alpha_char(state, state->storage[reg].alpha_packed[i]);
                }
            } else {
                char formatted[VGER_DISPLAY_STRING_LEN];
                int written = snprintf(formatted, sizeof(formatted), "%g", state->storage[reg].numeric);
                VGER_ASSERT(written >= 0);
                for (int i = 0; formatted[i] != '\0' && i < VGER_DISPLAY_STRING_LEN; i++) {
                    vger_handle_alpha_char(state, formatted[i]);
                }
            }
            break;
        default:
            break;
    }
}

static void vger_exec_flag_arg(vger_state_t *state, vger_key_id_t opcode, int flag, vger_exec_result_t *out)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(out != NULL);
    if (flag < 0 || flag > VGER_MAX_USER_FLAG) {
        return;
    }

    switch (opcode) {
        case VGER_KEY_SF:
            state->flags[flag] = true;
            break;
        case VGER_KEY_CF:
            state->flags[flag] = false;
            break;
        case VGER_KEY_FS_QUESTION_C:
            if (state->flags[flag]) {
                state->flags[flag] = false;
            } else {
                out->skip_next = true;
            }
            break;
        default:
            break;
    }
}

/** @brief GTO and XEQ both jump to a LBL target; XEQ additionally pushes
 *  a return address (the line after this step) onto the call stack first,
 *  so a later RTN resumes here. LBL itself is a marker, not an action. */
static void vger_exec_label_arg(vger_state_t *state, vger_key_id_t opcode, int label, int *io_current_line, vger_exec_result_t *out)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(io_current_line != NULL);
    VGER_ASSERT(out != NULL);

    if (opcode != VGER_KEY_GTO && opcode != VGER_KEY_XEQ) {
        return;
    }
    int target = -1;
    if (!vger_program_find_label(&state->program, label, &target)) {
        return;
    }
    if (opcode == VGER_KEY_XEQ && !vger_call_stack_push(state, *io_current_line + 1)) {
        out->halt = true; /* call-stack overflow: halt rather than corrupt state */
        return;
    }
    *io_current_line = target;
    out->jumped = true;
}

static void vger_exec_digits_arg(vger_state_t *state, vger_key_id_t opcode, int digits)
{
    VGER_ASSERT(state != NULL);
    if (digits < 0 || digits > 9) {
        return;
    }
    if (opcode == VGER_KEY_FIX) {
        state->display_format = VGER_DISPLAY_FIX;
    } else if (opcode == VGER_KEY_SCI) {
        state->display_format = VGER_DISPLAY_SCI;
    } else if (opcode == VGER_KEY_ENG) {
        state->display_format = VGER_DISPLAY_ENG;
    }
    state->display_digits = digits;
}

static vger_exec_result_t vger_execute_step(vger_state_t *state, const vger_program_step_t *step, int *io_current_line)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(step != NULL);
    VGER_ASSERT(io_current_line != NULL);

    vger_exec_result_t result;
    memset(&result, 0, sizeof(result));

    switch (step->kind) {
        case VGER_STEP_NUMBER_LITERAL:
            vger_stack_push_lift(state);
            state->reg_x = step->number_value;
            state->stack_lift_enabled = true;
            break;
        case VGER_STEP_ALPHA_LITERAL:
            state->alpha_len = 0;
            state->alpha_buffer[0] = '\0';
            for (int i = 0; step->alpha_value[i] != '\0' && i < VGER_ALPHA_BUFFER_LEN; i++) {
                vger_handle_alpha_char(state, step->alpha_value[i]);
            }
            break;
        case VGER_STEP_OPCODE_ONLY:
            vger_exec_opcode_only(state, step->opcode, io_current_line, &result);
            break;
        case VGER_STEP_OPCODE_REG_ARG:
            vger_exec_reg_arg(state, step->opcode, step->int_arg);
            break;
        case VGER_STEP_OPCODE_FLAG_ARG:
            vger_exec_flag_arg(state, step->opcode, step->int_arg, &result);
            break;
        case VGER_STEP_OPCODE_LABEL_ARG:
            vger_exec_label_arg(state, step->opcode, step->int_arg, io_current_line, &result);
            break;
        case VGER_STEP_OPCODE_DIGITS_ARG:
            vger_exec_digits_arg(state, step->opcode, step->int_arg);
            break;
        default:
            VGER_ASSERT(false);
            break;
    }

    VGER_ASSERT(result.halt == true || result.halt == false);
    return result;
}

/* ---------------------------------------------------------------------- */
/* R/S: run the stored program synchronously to completion                */
/* ---------------------------------------------------------------------- */

static void vger_run_program(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(!state->program_running);

    int line = (state->current_line >= 0) ? state->current_line : 0;
    state->program_running = true;
    state->call_stack_depth = 0; /* a fresh top-level run starts with no pending subroutines */
    bool pending_skip = false;

    for (long steps = 0; steps < VGER_MAX_STEPS_PER_RUN; steps++) {
        if (line < 0 || line >= state->program.step_count) {
            break;
        }

        if (pending_skip) {
            pending_skip = false;
            line++;
            continue;
        }

        vger_exec_result_t result = vger_execute_step(state, &state->program.steps[line], &line);
        if (result.halt) {
            break;
        }
        pending_skip = result.skip_next;
        if (!result.jumped) {
            line++;
        }
    }

    state->program_running = false;
    state->current_line = line;
    VGER_ASSERT(!state->program_running);
}

/* ---------------------------------------------------------------------- */
/* Top-level dispatch                                                     */
/* ---------------------------------------------------------------------- */

/** @brief Anything besides ON terminates while the calculator is off. */
static void vger_handle_power_key(vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    state->power_on = !state->power_on;
    VGER_ASSERT(state->power_on == true || state->power_on == false);
}

void vger_interp_handle_key(vger_state_t *state, vger_key_event_t event)
{
    VGER_ASSERT(state != NULL);
    VGER_ASSERT(event.id >= 0 && event.id < VGER_KEY_COUNT);

    if (event.id == VGER_KEY_ON) {
        vger_handle_power_key(state);
        return;
    }
    if (!state->power_on) {
        return;
    }
    if (event.id == VGER_KEY_PRGM) {
        vger_flush_digit_entry(state);
        state->calc_mode = (state->calc_mode == VGER_CALC_MODE_RUN) ? VGER_CALC_MODE_PRGM : VGER_CALC_MODE_RUN;
        return;
    }
    if (event.id == VGER_KEY_USER) {
        state->user_mode = !state->user_mode;
        return;
    }
    if (event.id == VGER_KEY_ALPHA) {
        vger_handle_alpha_toggle(state);
        return;
    }
    if (event.id == VGER_KEY_ALPHA_CHAR) {
        vger_handle_alpha_char(state, event.ch);
        return;
    }
    if (event.id == VGER_KEY_BACKSPACE) {
        vger_handle_backspace(state);
        return;
    }
    if (event.id == VGER_KEY_RUN_STOP) {
        vger_flush_digit_entry(state);
        if (!state->program_running) {
            vger_run_program(state);
        }
        return;
    }
    if (state->awaiting_argument) {
        vger_handle_argument_digit_or_abort(state, event);
        return;
    }
    if (event.id == VGER_KEY_CHS && state->entering_digits) {
        vger_toggle_entry_sign(state);
        return;
    }
    if (event.id == VGER_KEY_DECIMAL_POINT || (event.id >= VGER_KEY_DIGIT_0 && event.id <= VGER_KEY_DIGIT_9)) {
        vger_handle_digit_entry_key(state, event.id);
        return;
    }

    vger_flush_digit_entry(state);

    vger_arg_spec_entry_t spec;
    if (vger_lookup_arg_spec(event.id, &spec)) {
        state->awaiting_argument = true;
        state->awaiting_argument_for = event.id;
        state->argument_digit_count = 0;
        return;
    }

    vger_commit_instruction(state, VGER_STEP_OPCODE_ONLY, event.id, 0);
}
