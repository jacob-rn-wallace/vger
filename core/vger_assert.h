/**
 * @file vger_assert.h
 * @brief Assertion macro that stays active regardless of NDEBUG.
 *
 * Power-of-10 rule 5 calls out a specific footgun: C's standard assert()
 * silently compiles to nothing under -DNDEBUG, which CMake's default
 * "Release" build type defines automatically. VGER_ASSERT() does not use
 * <assert.h> and is never compiled out, so a Release build keeps the same
 * invariant checking as a debug build. Failure calls vger_assert_fail(),
 * which is the one place recovery behavior lives (currently: report and
 * abort; an embedded target can later swap this for a fault handler
 * without touching any call site).
 */

#ifndef VGER_ASSERT_H
#define VGER_ASSERT_H

/** @brief Report an assertion failure and terminate. Never returns.
 *
 * Declared _Noreturn (not just documented as such) so the compiler's flow
 * analysis knows control can't fall through a failed VGER_ASSERT() -
 * without this, GCC's -Wmaybe-uninitialized can't see that a variable only
 * assigned on the "assertion holds" path is always initialized by the time
 * execution continues past the assert.
 */
_Noreturn void vger_assert_fail(const char *condition_text, const char *file, int line);

/** @brief Evaluate cond; on failure, report and terminate via
 *  vger_assert_fail(). Always active, independent of NDEBUG. */
#define VGER_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            vger_assert_fail(#cond, __FILE__, __LINE__); \
        } \
    } while (0)

#endif /* VGER_ASSERT_H */
