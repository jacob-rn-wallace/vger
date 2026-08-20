/**
 * @file vger_mode_policy.h
 * @brief Swappable mode-boundary interruption policy (architecture
 * principle 3): decides whether a MENU-key press may take effect right
 * now, given what the calculator is currently doing.
 *
 * This is deliberately an enum dispatched through a single switch, not a
 * function-pointer table — see DEVIATIONS.md's "Rule 9" entry for why a
 * function-pointer "interface" pattern was rejected for this project's
 * own logic. Adding a future policy (deferred entry, always-live with
 * snapshot/restore) means adding an enum value and a switch case in
 * vger_mode_policy.c, not retrofitting call sites.
 *
 * MENU itself is not part of vger_key_id_t (architecture principle 2: the
 * extended/system menu layer sits on top of the core, never fused into
 * it). A host input loop (harness or firmware) intercepts the MENU key
 * before it would ever reach vger_interp_handle_key(), calls
 * vger_mode_policy_may_enter_menu() to decide whether to act on it, and
 * only then hands off to whatever menu system exists above the core.
 */

#ifndef VGER_MODE_POLICY_H
#define VGER_MODE_POLICY_H

#include <stdbool.h>

#include "vger_state.h"

/** @brief Identifies which mode-boundary policy is active. */
typedef enum {
    /** @brief MENU only takes effect when the calculator is idle: not
     *  mid-digit-entry, not mid-argument-entry, not running a program.
     *  The only policy implemented so far (per architecture principle 4).
     */
    VGER_MODE_POLICY_IDLE_ONLY = 0,
    VGER_MODE_POLICY_COUNT
} vger_mode_policy_id_t;

/**
 * @brief Decide whether MENU may be entered right now under policy.
 *
 * @param policy Which policy to evaluate.
 * @param state Current calculator state; queried only through the public
 *              state API, never touched directly.
 * @return true if MENU may take effect now, false if it should be ignored
 *         (or, for a future deferred-entry policy, queued) this call.
 */
bool vger_mode_policy_may_enter_menu(vger_mode_policy_id_t policy, const vger_state_t *state);

#endif /* VGER_MODE_POLICY_H */
