# Power of 10 — deviations

This project's own code follows NASA/JPL's "Power of 10" rules as strong
defaults, per the project's coding conventions. This is a hobbyist project,
not flight software — but the rules are good discipline, so we follow them
everywhere they're actually achievable. Deviations are recorded here rather
than silently ignored or faked: which rule, exactly what's excepted, why,
and the boundary of the exception.

This file follows the same format as the equivalent file in soynut, a
related project on the same hardware target.

## Rule 9 (no function pointers) — not deviated from

Worth recording as a *non*-exception: the mode-boundary interruption policy
(architecture principle 3 — swappable, idle-only for now) is implemented as
an enum (`vger_mode_policy_id_t`) dispatched through a single `switch` in
`vger_mode_policy_may_enter_menu()`, not as a function-pointer table. A
function-pointer "policy interface" is the more idiomatic C pattern for
this, but soynut's own DEVIATIONS.md is explicit that rule 9 exceptions
cover only *calling into* third-party SDK/framework callback APIs, never a
function-pointer table declared in this project's own logic ("if a rewrite
pass finds one of those, it gets redesigned instead of added here"). Adding
a policy later means adding an enum value and a switch case — that is what
"swappable" means here, not runtime-pluggable dispatch.

## Rule 2 (every loop must have a fixed, provable upper bound)

**Scope of the exception:** the top-level `while (running)` event loop in
`harness/main.c`.

**Why:** the same case soynut's own DEVIATIONS.md calls out for its
firmware `while (true)` loops: an interactive GUI has no natural
termination bound besides the user closing the window, which is exactly
the "intentionally nonterminating iteration, provably unable to terminate
[other than by external event]" case rule 2's own text anticipates. Every
loop *inside* one iteration of it (event draining, seven-segment string
rendering) has a real, explicit bound (`VGER_MAX_EVENTS_PER_FRAME`,
`VGER_DISPLAY_MAX_CHARS`).

**Boundary:** this covers exactly the one designated top-level loop in
`main()`, nothing nested inside it.

## Implementation note: `VGER_ASSERT` stays active regardless of `-DNDEBUG`

Not a deviation, but worth recording since it isn't obvious: CMake's default
Release build type defines `-DNDEBUG`, and `<assert.h>`'s `assert()`
compiles to nothing under `NDEBUG` — silently disabling every rule 5 check.
Rather than remembering to append `-UNDEBUG` per target (soynut's fix),
`core/vger_assert.h` defines `VGER_ASSERT()` from scratch, independent of
`<assert.h>` entirely, so it can never be compiled out by a build-type flag
in the first place.
