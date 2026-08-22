/**
 * @file vger_mode_policy.c
 * @brief Implementation of the mode-boundary interruption policy.
 */

#include "vger_mode_policy.h"

#include "vger_assert.h"

/** @brief Idle-only policy body: true iff nothing is mid-flight. */
static bool vger_policy_idle_only(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);

    bool idle = vger_get_power_on(state) && !vger_get_is_running(state) && !vger_get_is_entering_digits(state) &&
                !vger_get_is_awaiting_argument(state);

    VGER_ASSERT(idle == true || idle == false);
    return idle;
}

bool vger_mode_policy_may_enter_menu(vger_mode_policy_id_t policy, const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);
    /* No `policy >= 0` half here: enum vger_mode_policy_id_t has no
     * negative enumerators, so some compilers (e.g. GCC on ARM/AAPCS,
     * which picks an unsigned underlying type in that case) flag that
     * comparison as always-true under -Wtype-limits. */
    VGER_ASSERT(policy < VGER_MODE_POLICY_COUNT);

    switch (policy) {
        case VGER_MODE_POLICY_IDLE_ONLY:
            return vger_policy_idle_only(state);
        case VGER_MODE_POLICY_COUNT:
        default:
            VGER_ASSERT(false);
            return false;
    }
}
