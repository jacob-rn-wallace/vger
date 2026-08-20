/**
 * @file test_core.c
 * @brief Desktop unit tests for the vger FOCAL core, run without any
 * display/keypad hardware. Exercises the state/query API and interpreter
 * directly via synthetic key events, the same way the harness or a future
 * firmware input loop would drive them.
 *
 * Not a framework: a small check() helper (Power-of-10 rule 5's own
 * guidance on assert()-under-NDEBUG applies just as much to a test binary
 * as to the core, so this uses the same always-active pattern as
 * VGER_ASSERT rather than <assert.h>) plus a bounded list of test
 * functions run in sequence from main().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vger_interp.h"
#include "vger_mode_policy.h"
#include "vger_program.h"
#include "vger_state.h"

static int g_failures = 0;

/** @brief Report a failed expectation; does not abort so the rest of the
 *  test suite still runs and reports everything wrong in one pass. */
static void vger_test_fail(const char *test_name, const char *what, int line)
{
    (void)fprintf(stderr, "FAIL %s (test_core.c:%d): %s\n", test_name, line, what);
    g_failures++;
}

#define CHECK(test_name, cond, what) \
    do { \
        if (!(cond)) { \
            vger_test_fail((test_name), (what), __LINE__); \
        } \
    } while (0)

#define CHECK_NEAR(test_name, actual, expected, what) \
    do { \
        double diff = (actual) - (expected); \
        if (diff < 0) { \
            diff = -diff; \
        } \
        if (diff > 1e-9) { \
            (void)fprintf(stderr, "FAIL %s (test_core.c:%d): %s (got %g, want %g)\n", (test_name), __LINE__, (what), \
                           (double)(actual), (double)(expected)); \
            g_failures++; \
        } \
    } while (0)

static void vger_press(vger_state_t *state, vger_key_id_t id)
{
    vger_key_event_t event = {id, '\0'};
    vger_interp_handle_key(state, event);
}

static void vger_press_digit(vger_state_t *state, int digit)
{
    vger_press(state, (vger_key_id_t)(VGER_KEY_DIGIT_0 + digit));
}

/** @brief Press the two digits of a 00-99 argument (STO/RCL/SF/GTO/...). */
static void vger_press_two_digit_arg(vger_state_t *state, int value)
{
    vger_press_digit(state, (value / 10) % 10);
    vger_press_digit(state, value % 10);
}

static void test_arithmetic(void)
{
    const char *name = "test_arithmetic";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press_digit(state, 3);
    vger_press(state, VGER_KEY_ENTER);
    vger_press_digit(state, 4);
    vger_press(state, VGER_KEY_PLUS);

    CHECK_NEAR(name, vger_get_x(state), 7.0, "3 ENTER 4 + should leave 7 in X");
    CHECK_NEAR(name, vger_get_lastx(state), 4.0, "LASTX should hold the operand (4)");

    vger_press_digit(state, 2);
    vger_press(state, VGER_KEY_TIMES);
    CHECK_NEAR(name, vger_get_x(state), 14.0, "7 lifted, 2, * should leave 14 in X");
}

static void test_stack_lift_and_enter(void)
{
    const char *name = "test_stack_lift_and_enter";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press_digit(state, 5);
    vger_press(state, VGER_KEY_ENTER);
    vger_press_digit(state, 3);
    vger_press(state, VGER_KEY_ENTER);
    vger_press_digit(state, 2);

    CHECK_NEAR(name, vger_get_x(state), 2.0, "X should be 2 after 5 ENTER 3 ENTER 2");
    CHECK_NEAR(name, vger_get_y(state), 3.0, "Y should be 3 (ENTER disables lift for the next entry)");
    CHECK_NEAR(name, vger_get_z(state), 5.0, "Z should be 5");
}

static void test_sto_rcl(void)
{
    const char *name = "test_sto_rcl";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press_digit(state, 9);
    vger_press(state, VGER_KEY_STO);
    vger_press_two_digit_arg(state, 5);

    vger_press(state, VGER_KEY_CLX);
    CHECK_NEAR(name, vger_get_x(state), 0.0, "CLX should zero X");

    vger_press(state, VGER_KEY_RCL);
    vger_press_two_digit_arg(state, 5);
    CHECK_NEAR(name, vger_get_x(state), 9.0, "RCL 05 should restore the stored 9");
    CHECK(name, vger_get_storage_kind(state, 5) == VGER_REG_NUMERIC, "register 05 should be numeric");
}

static void test_indirect_sto_rcl(void)
{
    const char *name = "test_indirect_sto_rcl";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press_digit(state, 2);
    vger_press_digit(state, 0);
    vger_press(state, VGER_KEY_STO);
    vger_press_two_digit_arg(state, 5); /* register 05 = 20 (the pointer) */

    vger_press_digit(state, 4);
    vger_press_digit(state, 2);
    vger_press(state, VGER_KEY_STO);
    vger_press(state, VGER_KEY_IND);
    vger_press_two_digit_arg(state, 5); /* STO IND 05: really stores into register 20 */

    CHECK_NEAR(name, vger_get_storage_numeric(state, 20), 42.0, "STO IND 05 should store into register 20 (05's contents)");
    CHECK_NEAR(name, vger_get_storage_numeric(state, 5), 20.0, "register 05 itself (the pointer) should be untouched");

    vger_press(state, VGER_KEY_CLX);
    vger_press(state, VGER_KEY_RCL);
    vger_press(state, VGER_KEY_IND);
    vger_press_two_digit_arg(state, 5); /* RCL IND 05: really recalls from register 20 */

    CHECK_NEAR(name, vger_get_x(state), 42.0, "RCL IND 05 should recall register 20's contents");
}

static void test_indirect_gto(void)
{
    const char *name = "test_indirect_gto";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    const char *source = "LBL 01\n"
                          "2\n"
                          "STO 07\n"
                          "GTO IND 07\n"
                          "STO 99\n"
                          "LBL 02\n"
                          "9\n"
                          "STO 98\n"
                          "END\n";

    vger_program_t program;
    char err[128];
    bool parsed = vger_program_parse_text(source, &program, err, sizeof(err));
    CHECK(name, parsed, err);
    if (!parsed) {
        return;
    }

    vger_state_load_program(state, &program);
    vger_press(state, VGER_KEY_RUN_STOP);

    CHECK_NEAR(name, vger_get_storage_numeric(state, 98), 9.0, "GTO IND 07 should jump to LBL 02 (07 holds 2)");
    CHECK(name, vger_get_storage_kind(state, 99) == VGER_REG_NUMERIC && vger_get_storage_numeric(state, 99) == 0.0,
          "register 99 should be untouched: GTO IND jumped over the STO 99 step");
}

static void test_comparison_skip_vs_y(void)
{
    const char *name = "test_comparison_skip_vs_y";
    vger_state_t *state = vger_state_get();
    char err[128];

    /* X(3) > Y(5) is false: X>Y? should skip the following GTO 02, so
     * STO 03 runs and register 03 ends up holding X (3). */
    const char *source_false = "LBL 01\n"
                                "5\n"
                                "STO 00\n"
                                "3\n"
                                "STO 01\n"
                                "RCL 00\n"
                                "RCL 01\n"
                                "X>Y?\n"
                                "GTO 02\n"
                                "STO 03\n"
                                "LBL 02\n"
                                "END\n";
    vger_program_t program_false;
    bool parsed_false = vger_program_parse_text(source_false, &program_false, err, sizeof(err));
    CHECK(name, parsed_false, err);
    if (parsed_false) {
        vger_state_reset(state);
        vger_state_load_program(state, &program_false);
        vger_press(state, VGER_KEY_RUN_STOP);
        CHECK_NEAR(name, vger_get_storage_numeric(state, 3), 3.0, "X>Y? false should skip GTO 02, letting STO 03 run");
    }

    /* X(5) > Y(3) is true: X>Y? should NOT skip, so GTO 02 jumps clean
     * over STO 03 and register 03 stays untouched (0). */
    const char *source_true = "LBL 01\n"
                               "3\n"
                               "STO 00\n"
                               "5\n"
                               "STO 01\n"
                               "RCL 00\n"
                               "RCL 01\n"
                               "X>Y?\n"
                               "GTO 02\n"
                               "STO 03\n"
                               "LBL 02\n"
                               "END\n";
    vger_program_t program_true;
    bool parsed_true = vger_program_parse_text(source_true, &program_true, err, sizeof(err));
    CHECK(name, parsed_true, err);
    if (parsed_true) {
        vger_state_reset(state);
        vger_state_load_program(state, &program_true);
        vger_press(state, VGER_KEY_RUN_STOP);
        CHECK(name, vger_get_storage_kind(state, 3) == VGER_REG_NUMERIC && vger_get_storage_numeric(state, 3) == 0.0,
              "X>Y? true should not skip GTO 02, jumping clean over STO 03");
    }
}

static void test_comparison_skip_vs_zero(void)
{
    const char *name = "test_comparison_skip_vs_zero";
    vger_state_t *state = vger_state_get();
    char err[128];

    /* X=0? true (X really is 0): should not skip, so GTO 02 jumps clean
     * over "99 STO 05" and register 05 keeps its sentinel value (7). */
    const char *source_true = "LBL 01\n"
                               "7\n"
                               "STO 05\n"
                               "0\n"
                               "X=0?\n"
                               "GTO 02\n"
                               "99\n"
                               "STO 05\n"
                               "LBL 02\n"
                               "END\n";
    vger_program_t program_true;
    bool parsed_true = vger_program_parse_text(source_true, &program_true, err, sizeof(err));
    CHECK(name, parsed_true, err);
    if (parsed_true) {
        vger_state_reset(state);
        vger_state_load_program(state, &program_true);
        vger_press(state, VGER_KEY_RUN_STOP);
        CHECK_NEAR(name, vger_get_storage_numeric(state, 5), 7.0, "X=0? true should not skip, jumping clean over STO 05");
    }

    /* X=0? false (X is 3): should skip the GTO 02, letting "99 STO 05"
     * run, so register 05 ends up holding 99. */
    const char *source_false = "LBL 01\n"
                                "7\n"
                                "STO 05\n"
                                "3\n"
                                "X=0?\n"
                                "GTO 02\n"
                                "99\n"
                                "STO 05\n"
                                "LBL 02\n"
                                "END\n";
    vger_program_t program_false;
    bool parsed_false = vger_program_parse_text(source_false, &program_false, err, sizeof(err));
    CHECK(name, parsed_false, err);
    if (parsed_false) {
        vger_state_reset(state);
        vger_state_load_program(state, &program_false);
        vger_press(state, VGER_KEY_RUN_STOP);
        CHECK_NEAR(name, vger_get_storage_numeric(state, 5), 99.0, "X=0? false should skip GTO 02, letting STO 05 run");
    }
}

static void test_asto_arcl(void)
{
    const char *name = "test_asto_arcl";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press(state, VGER_KEY_ALPHA);
    vger_interp_handle_key(state, (vger_key_event_t){VGER_KEY_ALPHA_CHAR, 'H'});
    vger_interp_handle_key(state, (vger_key_event_t){VGER_KEY_ALPHA_CHAR, 'I'});
    vger_press(state, VGER_KEY_ASTO);
    vger_press_two_digit_arg(state, 10);
    vger_press(state, VGER_KEY_ALPHA); /* exit alpha; ASTO already ran, so this just toggles the mode */

    CHECK(name, vger_get_storage_kind(state, 10) == VGER_REG_ALPHA_PACKED, "register 10 should hold packed alpha");

    char packed[VGER_ALPHA_PACK_LEN];
    vger_get_storage_alpha(state, 10, packed);
    CHECK(name, packed[0] == 'H' && packed[1] == 'I', "packed alpha should start with HI");
}

static void test_program_run_via_text_load(void)
{
    const char *name = "test_program_run_via_text_load";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    const char *source = "LBL 01\n"
                          "3\n"
                          "STO 00\n"
                          "4\n"
                          "STO 01\n"
                          "RCL 00\n"
                          "RCL 01\n"
                          "+\n"
                          "STO 02\n"
                          "SF 05\n"
                          "FS?C 05\n"
                          "GTO 02\n"
                          "STO 03\n"
                          "LBL 02\n"
                          "END\n";

    vger_program_t program;
    char err[128];
    bool parsed = vger_program_parse_text(source, &program, err, sizeof(err));
    CHECK(name, parsed, err);
    if (!parsed) {
        return;
    }
    CHECK(name, program.step_count == 15, "expected 15 parsed steps");

    vger_state_load_program(state, &program);
    vger_press(state, VGER_KEY_RUN_STOP);

    CHECK_NEAR(name, vger_get_storage_numeric(state, 2), 7.0, "register 02 should hold 3+4=7");
    CHECK(name, vger_get_flag(state, 5) == false, "flag 05 should have been cleared by FS?C");
    CHECK(name, vger_get_storage_kind(state, 3) == VGER_REG_NUMERIC && vger_get_storage_numeric(state, 3) == 0.0,
          "register 03 should be untouched (0): GTO 02 jumped over the STO 03 step");
}

static void test_xeq_rtn_subroutine(void)
{
    const char *name = "test_xeq_rtn_subroutine";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    const char *source = "LBL 01\n"
                          "5\n"
                          "XEQ 02\n"
                          "STO 00\n"
                          "END\n"
                          "LBL 02\n"
                          "3\n"
                          "+\n"
                          "RTN\n";

    vger_program_t program;
    char err[128];
    bool parsed = vger_program_parse_text(source, &program, err, sizeof(err));
    CHECK(name, parsed, err);
    if (!parsed) {
        return;
    }

    vger_state_load_program(state, &program);
    vger_press(state, VGER_KEY_RUN_STOP);

    CHECK_NEAR(name, vger_get_storage_numeric(state, 0), 8.0,
               "XEQ 02 should call the 5+3 subroutine and RTN should resume at STO 00");
    CHECK(name, vger_get_call_stack_depth(state) == 0, "call stack should be empty again after RTN");
}

static void test_xeq_call_stack_overflow_halts(void)
{
    const char *name = "test_xeq_call_stack_overflow_halts";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    /* Self-recursive XEQ with no base case: each call pushes a return
     * address, so the call stack fills up and execution halts once it
     * hits VGER_CALL_STACK_MAX_DEPTH, rather than corrupting state or
     * running away (bounded by the step cap regardless). */
    const char *source = "LBL 01\n"
                          "XEQ 01\n"
                          "END\n";

    vger_program_t program;
    char err[128];
    bool parsed = vger_program_parse_text(source, &program, err, sizeof(err));
    CHECK(name, parsed, err);
    if (!parsed) {
        return;
    }

    vger_state_load_program(state, &program);
    vger_press(state, VGER_KEY_RUN_STOP);

    CHECK(name, vger_get_call_stack_depth(state) == VGER_CALL_STACK_MAX_DEPTH,
          "unbounded recursive XEQ should halt exactly at the call-stack depth cap");
}

static void test_prgm_mode_keystroke_recording(void)
{
    const char *name = "test_prgm_mode_keystroke_recording";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press(state, VGER_KEY_PRGM);
    vger_press(state, VGER_KEY_LBL);
    vger_press_two_digit_arg(state, 1);
    vger_press_digit(state, 6);
    vger_press_digit(state, 6);
    vger_press(state, VGER_KEY_STO);
    vger_press_two_digit_arg(state, 20);
    vger_press(state, VGER_KEY_END);
    vger_press(state, VGER_KEY_PRGM); /* back to RUN */

    CHECK(name, vger_get_program_step_count(state) == 4, "expected 4 recorded steps (LBL, number, STO, END)");
    CHECK(name, vger_get_calc_mode(state) == VGER_CALC_MODE_RUN, "should be back in RUN mode");

    vger_press(state, VGER_KEY_RUN_STOP);
    CHECK_NEAR(name, vger_get_storage_numeric(state, 20), 66.0, "recorded program should store 66 into register 20");
}

static void test_mode_policy_idle_only(void)
{
    const char *name = "test_mode_policy_idle_only";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    CHECK(name, vger_mode_policy_may_enter_menu(VGER_MODE_POLICY_IDLE_ONLY, state), "idle calculator should allow MENU");

    vger_press_digit(state, 1); /* mid-digit-entry now */
    CHECK(name, !vger_mode_policy_may_enter_menu(VGER_MODE_POLICY_IDLE_ONLY, state),
          "mid-digit-entry should refuse MENU");
}

static void test_out_of_band_reset(void)
{
    const char *name = "test_out_of_band_reset";
    vger_state_t *state = vger_state_get();
    vger_state_reset(state);

    vger_press_digit(state, 9);
    vger_press(state, VGER_KEY_STO);
    vger_press_two_digit_arg(state, 0);

    vger_state_reset(state); /* the out-of-band path: never goes through vger_interp_handle_key() */

    CHECK_NEAR(name, vger_get_x(state), 0.0, "reset should zero X");
    CHECK_NEAR(name, vger_get_storage_numeric(state, 0), 0.0, "reset should clear storage registers too");
}

int main(void)
{
    test_arithmetic();
    test_stack_lift_and_enter();
    test_sto_rcl();
    test_asto_arcl();
    test_program_run_via_text_load();
    test_xeq_rtn_subroutine();
    test_xeq_call_stack_overflow_halts();
    test_indirect_sto_rcl();
    test_indirect_gto();
    test_comparison_skip_vs_y();
    test_comparison_skip_vs_zero();
    test_prgm_mode_keystroke_recording();
    test_mode_policy_idle_only();
    test_out_of_band_reset();

    if (g_failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    (void)fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
