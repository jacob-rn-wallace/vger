/**
 * @file vger_keymap.h
 * @brief Maps SDL2 keyboard scancodes onto the core's logical key
 * vocabulary (vger_key_id_t) and, separately, onto host-level actions
 * (MENU, RESET) that never reach the core at all.
 *
 * This mapping is entirely a harness concern (architecture principle 2:
 * MENU is not in vger_key_id_t) and an out-of-band concern (principle 5:
 * RESET is not a key the core dispatcher ever sees).
 */

#ifndef VGER_KEYMAP_H
#define VGER_KEYMAP_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "vger_types.h"

/** @brief What a raw keyboard scancode means at the host level, before
 *  any of it reaches the core interpreter. */
typedef enum {
    VGER_HOST_ACTION_NONE = 0,   /**< Not bound to anything. */
    VGER_HOST_ACTION_CORE_KEY,   /**< Forward as a vger_key_event_t. */
    VGER_HOST_ACTION_MENU,       /**< The 5th key: gated by the mode policy, never sent to the core. */
    VGER_HOST_ACTION_RESET       /**< Out-of-band recovery: calls vger_state_reset() directly. */
} vger_host_action_t;

/**
 * @brief Classify one SDL scancode.
 *
 * @param scancode Raw scancode from an SDL_KEYDOWN event.
 * @param alpha_mode_active Whether the calculator's ALPHA mode is
 *        currently on (queried by the caller via vger_get_annunciators()
 *        before calling this) — while true, printable keys are routed to
 *        VGER_KEY_ALPHA_CHAR instead of their normal function meaning.
 * @param out_event Filled in when the return value is
 *        VGER_HOST_ACTION_CORE_KEY.
 * @return Which host-level action this scancode maps to.
 */
vger_host_action_t vger_keymap_classify(SDL_Scancode scancode, bool alpha_mode_active, vger_key_event_t *out_event);

#endif /* VGER_KEYMAP_H */
