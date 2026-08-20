/**
 * @file vger_interp.h
 * @brief Key-dispatch entry point: the core's only mutation path besides
 * vger_state_reset() (architecture principle 5 keeps reset out-of-band and
 * separate from this).
 */

#ifndef VGER_INTERP_H
#define VGER_INTERP_H

#include "vger_state.h"
#include "vger_types.h"

/**
 * @brief Handle one key event against state.
 *
 * In VGER_CALC_MODE_RUN, most keys execute immediately. In
 * VGER_CALC_MODE_PRGM, most keys are recorded as a new program step
 * instead (milestone 1 records append-only, at the end of the program;
 * navigating to an arbitrary line to insert/edit is deferred). R/S always
 * executes immediately regardless of mode: it runs the stored program
 * synchronously, to completion, starting at the current line.
 *
 * @param state Calculator state to mutate.
 * @param event The key event to process.
 */
void vger_interp_handle_key(vger_state_t *state, vger_key_event_t event);

#endif /* VGER_INTERP_H */
