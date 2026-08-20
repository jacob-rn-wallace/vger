/**
 * @file vger_assert.c
 * @brief Implementation of vger_assert_fail().
 */

#include "vger_assert.h"

#include <stdio.h>
#include <stdlib.h>

/** @brief Report an assertion failure to stderr and abort the process.
 *
 * @param condition_text Source text of the failed condition.
 * @param file Source file the assertion fired in.
 * @param line Source line the assertion fired on.
 */
_Noreturn void vger_assert_fail(const char *condition_text, const char *file, int line)
{
    if (condition_text == NULL) {
        condition_text = "<null condition text>";
    }
    if (file == NULL) {
        file = "<unknown file>";
    }

    (void)fprintf(stderr, "VGER_ASSERT failed: %s (%s:%d)\n", condition_text, file, line);
    abort();
}
