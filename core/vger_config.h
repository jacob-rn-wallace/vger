/**
 * @file vger_config.h
 * @brief Compile-time limits for the vger FOCAL core.
 *
 * Every bounded loop and fixed-size buffer in core/ derives its bound from
 * a constant defined here (Power-of-10 rule 2: no loop may terminate only
 * because some external buffer happens to be finite; the bound must be
 * named and explicit).
 */

#ifndef VGER_CONFIG_H
#define VGER_CONFIG_H

/** @brief Number of numeric/alpha storage registers, addressed 00-99. */
#define VGER_NUM_STORAGE_REGS 100

/** @brief Number of flags, addressed 00-99. Milestone 1 only allows the
 *  user (SF/CF/FS?C) to set/clear flags 00-29; 30-99 are reserved and
 *  read back as false until system-flag semantics are implemented. */
#define VGER_NUM_FLAGS 100

/** @brief Highest user-settable flag number (inclusive) in milestone 1. */
#define VGER_MAX_USER_FLAG 29

/** @brief Length of the ALPHA text buffer, matching the real HP-41. */
#define VGER_ALPHA_BUFFER_LEN 24

/** @brief Number of characters ASTO/ARCL pack into one storage register. */
#define VGER_ALPHA_PACK_LEN 6

/** @brief Maximum number of program lines a program may contain. */
#define VGER_PROGRAM_MAX_LINES 500

/** @brief Maximum characters in one program line's mnemonic text, including
 *  the terminating NUL. */
#define VGER_PROGRAM_LINE_TEXT_LEN 32

/** @brief Hard cap on instruction steps executed by a single R/S run.
 *
 * Documented FOCAL programs may loop indefinitely by design (that is a
 * feature, not a bug, in a Turing-complete language), so no fixed bound on
 * "the algorithm" can be proven. Rule 2 still requires an explicit bound:
 * this cap turns an unbounded/runaway program into a detectable, recoverable
 * error instead of a hang, which is the correct reading of the rule for an
 * interpreter whose whole job is running caller-supplied loops.
 */
#define VGER_MAX_STEPS_PER_RUN 100000

/** @brief Maximum characters in a rendered display string, including NUL. */
#define VGER_DISPLAY_STRING_LEN 32

/** @brief Maximum nested XEQ subroutine calls (the RTN return-address
 *  stack depth).
 *
 * Real HP-41 subroutine nesting is memory-dependent, not a fixed spec
 * number; milestone 1 doesn't model byte-exact program memory (see
 * CLAUDE.md's "Program representation" section), so this is a deliberately
 * chosen, named bound rather than a claimed hardware figure. XEQ beyond
 * this depth halts the run (see vger_call_stack_push()) instead of
 * silently corrupting state - the same "refuse rather than overflow"
 * posture as VGER_MAX_STEPS_PER_RUN.
 */
#define VGER_CALL_STACK_MAX_DEPTH 8

#endif /* VGER_CONFIG_H */
