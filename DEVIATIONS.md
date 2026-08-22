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
`harness/main.c`, and the top-level `while (true)` loop in
`firmware/main.c`.

**Why:** the same case soynut's own DEVIATIONS.md calls out for its
firmware `while (true)` loops: an interactive GUI (the harness) or a
running microcontroller (the firmware) has no natural termination bound
besides the user closing the window or powering the device off, which is
exactly the "intentionally nonterminating iteration, provably unable to
terminate [other than by external event]" case rule 2's own text
anticipates. Every loop *inside* one iteration of the harness's loop
(event draining, seven-segment string rendering) has a real, explicit
bound (`VGER_MAX_EVENTS_PER_FRAME`, `VGER_DISPLAY_MAX_CHARS`); the
firmware's `main()` runs its one-shot checkerboard bring-up test
(bounded by `VGER_LCD_WIDTH_PX`/`VGER_LCD_HEIGHT_PX`) once, before ever
reaching the unbounded loop.

**Boundary:** this covers exactly the one designated top-level loop in
each of those two `main()` functions, nothing nested inside them.

## License note: `firmware/st7920.c`/`.h` and `firmware/pins.h` ported from `soynut`

**What's excepted:** these three files are a near-verbatim port of
`soynut`'s `firmware/st7920.c`/`.h` and `firmware/pins.h` (same author),
renamed onto vger's `vger_`/`VGER_` symbol convention and switched from
bare `assert()` to `VGER_ASSERT()` (see the "`VGER_ASSERT` stays active
regardless of `-DNDEBUG`" note below) — otherwise unchanged: same GDRAM
addressing, same power-on init sequence and timing, same GPIO pin
assignment.

**Why this needed a decision, not just a copy:** `soynut` is licensed
GPL-2.0 as a whole repository (from vendoring GPLv2 `emu41gcc` as a
load-bearing dependency — see CLAUDE.md's "Compatibility target" and
"NHD14432/ST7920 driver" sections), while `vger` is Apache-2.0. Every
other cross-project reference in this codebase (Nonpareil, DB48X, C47,
WouoUI) is someone *else's* copyleft code and stays read-only, never
vendored. This case is different: `st7920.c`/`.h`/`pins.h` are the same
author's own original files, not third-party code caught up in
`soynut`'s GPL-2.0 status — so porting them into an Apache-2.0 project is
the rights-holder's call to make directly, not a license violation to
work around. Recorded here anyway, per CLAUDE.md's own rule that any
code crossing a license boundary gets a DEVIATIONS.md-style writeup
before it happens, not after.

**Boundary:** covers exactly these three files. Any other `soynut` file
pulled into `vger` later (e.g. its font tables, if the MENU/system-menu
layer's rendering ever needs them) needs its own entry here, not an
extension of this one.

## Implementation note: `VGER_ASSERT` stays active regardless of `-DNDEBUG`

Not a deviation, but worth recording since it isn't obvious: CMake's default
Release build type defines `-DNDEBUG`, and `<assert.h>`'s `assert()`
compiles to nothing under `NDEBUG` — silently disabling every rule 5 check.
Rather than remembering to append `-UNDEBUG` per target (soynut's fix),
`core/vger_assert.h` defines `VGER_ASSERT()` from scratch, independent of
`<assert.h>` entirely, so it can never be compiled out by a build-type flag
in the first place.
