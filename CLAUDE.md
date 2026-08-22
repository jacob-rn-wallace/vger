# CLAUDE.md

This file is the complete orientation document for this repository. It
should be sufficient on its own — an agent working in this repo shouldn't
need to also read README.md to understand what the project is, why it's
built the way it is, or what conventions to follow. (README.md exists too,
for humans landing on the repo page; keep the two in sync when either
changes, but this file is authoritative on anything they'd disagree about.)

## What this is

`vger` is an HP-41-inspired native calculator system — **not** a hardware
emulator. It targets the *interaction model* of the HP-41's alphanumeric
FOCAL-style interface (its simplicity and elegance), reimplemented on
modern hardware/software and extended further than the original Nut CPU
chip ever could.

### Name

V'Ger, from *Star Trek: The Motion Picture* — a simple, purpose-built probe
(Voyager 6) transformed by an alien machine intelligence into something
vastly beyond its original design, still recognizably itself at the core.
That's the intended spirit of this project: take the HP-41's simple,
elegant interaction model and push it further than the original hardware
ever could, without losing what made it simple and elegant in the first
place. It also continues HP's own tradition of space-probe calculator
names (the Voyager series — HP-11C/12C/15C/16C — the Pioneer series, the
Saturn CPU architecture) and this author's own convention of naming
calculator projects after space probes — `Cassini` (a separate project: a
ground-up HP Saturn-CPU-family replica, i.e. HP48/49/50-series, named for
the probe that orbited Saturn) being the other one so far. `soynut` (an
HP-41CV replica — its name is unrelated to the space-probe convention) is
a different related project, on the same Pico 2 + NHD14432/ST7920
144×32-display hardware target (see "Hardware target" below), whose
*architecture conventions* (not name) this project draws on — see
"Architecture principles" and "Coding conventions" below.

### Conceptual model: an HP-28C's capability on an HP-41C's keyboard

The target isn't just "FOCAL on new hardware" — it's an HP-28C's UI
ambition (a real menu system, and graphing) delivered through an HP-41C's
keyboard, with the top row replaced by the 5 soft keys (see "Hardware
target" below). This is a deliberately achievable bar, not an aspirational
one: the HP-28C's own display was 137×32 dot-matrix, almost identical to
the 144×32 target display here, and it shipped a full menu system and
graphing on that resolution — proof the constraint is tractable, not a
hope. HP-28C hardware/firmware isn't studied as code anywhere in this
project (no ROM image or emulator source is vendored or referenced) — it's
a feasibility precedent, not a code reference, so unlike Nonpareil/DB48X/
C47/WouoUI below, it carries no licensing weight at all.

### Compatibility target — what's in scope and what isn't

**In scope:** documented FOCAL programs — the standard, published HP-41
instruction/function set. This is a bounded, tractable target.

**Explicitly out of scope:**
- Byte-exact real-ROM execution (running actual HP-41 ROM images).
- Third-party accessory ROM binary compatibility.
- Synthetic programming (illegal-opcode tricks, undocumented behavior).
- Nut-CPU microcode fidelity.

Two architectures were deliberately rejected for this reason: a faithful
Nut-CPU instruction-level interpreter (what SwissMicros' DM41X and the
41CL project both do — their entire value proposition is byte-for-byte ROM
compatibility, which this project doesn't need), and a hybrid
native-engine-with-bytecode-fallback design.

Eric Smith's **Nonpareil** (GPLv2, <https://github.com/brouhaha/nonpareil>)
is used as a *reference* for understanding documented FOCAL/Nut-series
behavior — studied for understanding, never adapted or copied directly.
No Nonpareil code exists anywhere in this repository. If that ever changes
deliberately, it means deliberately taking on GPLv2 obligations for
whatever component copies from it, and that decision (and its scope) must
be recorded in DEVIATIONS.md-style detail before it happens, not
discovered after the fact.

Two other projects sit in the same reference-only category, useful for
FOCAL-adjacent menu/UI concepts rather than to `core/`: **DB48X**
(LGPL-3.0, <https://github.com/c3d/db48x>), a from-scratch RPL runtime in
the spirit of HP48/49/50, and **C47** (GPL-3.0,
<https://gitlab.com/rpncalculators/c43>), an HP-42S-lineage RPN
calculator. Both run on SwissMicros DM42/DM32 hardware via SwissMicros'
DMCP platform SDK, rendering onto the Sharp LS027B7DH01, 400×240 Memory
LCD — a larger, different display than vger's own hardware target (see
"Hardware target" below), so their pixel-level rendering technique isn't
directly reusable the way it once looked like it might be. Their
interaction-model choices (soft-key row layout, menu depth/navigation,
what's exposed as a shifted vs. dedicated function) are still legitimate
study material — see "MENU UI design constraints" below, informed by
hands-on use of both. Neither is a source of code to vendor or adapt:
both are copyleft (LGPL-3.0 and GPL-3.0 respectively), their UI code is
coupled to DMCP (which doesn't exist for the Pico 2), and DB48X's RPL
object-runtime is conceptually closer to `Cassini` (HP48/49/50
Saturn-family) than to this project's FOCAL model in any case. Same rule
as Nonpareil: read for ideas, never copy, and any deliberate exception
gets a DEVIATIONS.md-style writeup before it happens.

## Architecture principles (non-negotiable)

These came from hard lessons on `soynut` (a related project on the same
hardware target, where the display layer had to reverse-engineer raw
memory offsets because no query API existed — every extension since has
been painful because of it). Do not compromise on these without a
deliberate, discussed decision:

1. **The core exposes a clean, documented state/query API — nothing reaches
   into internal representation directly.** `core/vger_state.h` is the
   *only* way any consumer (display code, a future menu system, a
   debugger, the desktop harness) may observe calculator state. Extending
   what's observable means adding a query function to that header, never
   poking a new field through from outside `core/`.
2. **Native-41 interaction and any extended/system menu layer are
   separate.** The base interaction model must work standalone, fully
   usable without ever touching extended functionality. An extended layer
   is built *on top of* the core's query API — never fused into the core.
   Concretely: MENU is not a member of `vger_key_id_t` (the core's key
   vocabulary). A host input loop intercepts the MENU key before it ever
   reaches `vger_interp_handle_key()`.
3. **The mode-boundary interruption policy (what happens if MENU is
   pressed mid-operation) is a swappable policy, chosen at day one even
   though only the simplest option is implemented.** Not hard-coded inline
   in key-press handling. See "Mode-boundary policy" below for the actual
   mechanism and why it isn't a function-pointer table.
4. **The only implemented policy is idle-only.** MENU only takes effect
   when the calculator isn't mid-digit-entry, mid-argument-entry, or
   running a program. Deferred until there's a concrete reason to need
   them: deferred-entry and always-live-with-snapshot/restore policies.
5. **Recovery/escape (reset) is out-of-band, not part of the soft-key
   system.** `vger_state_reset()` is a plain function call, not a member of
   `vger_key_id_t` — structurally unreachable through
   `vger_interp_handle_key()`'s normal dispatch. In the eventual hardware
   version this models a physical reset switch separate from the keyboard
   matrix (SwissMicros DM42-style), not a reserved key combination
   competing with normal input handling. The desktop harness wires it to a
   dedicated key (F12) that calls `vger_state_reset()` directly, bypassing
   the interpreter entirely.

## Hardware target (future milestone, not yet built)

- MCU: Raspberry Pi Pico 2 (RP2350) — same platform as `soynut`, to reuse
  that toolchain/experience.
- Display: Newhaven NHD-14432WG-BTFH-VT, 144×32 graphic LCD (ST7920
  controller), replacing the original 14-segment display. This reverses
  an earlier direction — this file previously targeted the Sharp Memory
  LCD (LS027B7DH01, 400×240) — back onto the exact part `soynut` already
  drives in hardware; see "NHD14432/ST7920 driver" below. The desktop
  harness's logical render resolution (`VGER_LOGICAL_WIDTH`/`HEIGHT` in
  `harness/main.c`, now 144×32) and the seven-segment renderer's digit
  geometry (`vger_render_frame()`'s call into `vger_sevenseg_draw_string()`)
  have been reworked to match: 12 character cells at a 12px pitch exactly
  fill the 144px width and the annunciator row sits at y=21-25, both
  borrowed directly from soynut's own measured NHD14432 layout
  (`soynut/font-tables/hp41_pixel_segment_map.json`,
  `hp41_annunciator_pixel_map.json`) rather than picked from scratch — see
  `vger_render_frame()`'s doc comment for the exact numbers.
- Input: the original top-row 4 keys (ON / USER / PRGM / ALPHA) plus one
  new 5th key where the original overlay latch was. In native mode the
  outer 4 keys behave exactly as the originals; the middle key is MENU,
  shifting the outer 4 into navigation/selection for a system menu.

None of this hardware bring-up exists yet. `firmware/` is an empty
directory reserved for it. Milestone 1 (this snapshot) is desktop-only.

### NHD14432/ST7920 driver — `soynut` already has this, don't rebuild it

`soynut` (`/Users/jake/soynut`, same author; GPL-2.0 as a whole repo,
from vendoring GPLv2 `emu41gcc` as a load-bearing core dependency — a
coupling this project's native-reimplementation architecture was
deliberately designed to avoid, see "Compatibility target" above) already
has a complete, hardware-validated driver for this exact display part:
`firmware/st7920.c`/`.h` (low-level 8-bit parallel ST7920 driver) plus
`pins.h`'s GPIO pin table, with the full bring-up story — level-shifter
wiring for the 10 parallel signals, an ST7920-vs-NHD-datasheet chip-select
polarity discrepancy resolved in the controller datasheet's favor, and
power-on init sequence timing — written up in `soynut/CLAUDE.md` and
`soynut/DEVLOG.md`, with the actual datasheets in
`soynut/reference-material/datasheets/` (`ST7920.pdf`,
`NHD-14432WG-BTFH-VT.pdf`). The part supports both 8-bit parallel (the
active, working path in `soynut`) and 3-wire serial (implemented once,
now dormant — see `soynut/CLAUDE.md`'s "Direct Pico→LCD serial link"
history) via a board jumper.

`st7920.c`/`.h` are `soynut`'s own original files, not vendored
third-party code — so porting them into `vger` when `firmware/` bring-up
starts is a copyright decision the author can make directly, unlike the
Nonpareil/DB48X/C47/WouoUI entries above, which are someone else's
copyleft code. It still crosses from a GPL-2.0 repo into an Apache-2.0
one, so record the actual port (verbatim copy vs. rewritten from the same
design) in `DEVIATIONS.md`-style detail when it happens, same as every
other "code crossed a license boundary" decision in this file — just a
much lower-friction one than the others, since there's only one
rights-holder to satisfy. Same caveat as the Sharp-LCD candidates this
replaces: `st7920.c`/`.h` are the low-level GDRAM/bus layer, not a UI —
this project's own display/annunciator rendering (principle 1: driven
entirely through `vger_state.h`'s query API) still sits on top of it.

## Repository layout

```
core/       FOCAL interpreter core. Zero hardware/SDK dependencies -
            builds and links identically on desktop and (later) Pico 2.
harness/    Desktop SDL2 test harness - milestone 1's proof-of-architecture
            target, not a preview of the real UI.
tests/      Desktop unit tests exercising core/ directly via synthetic key
            events. No display/keypad hardware needed to run these.
firmware/   Reserved for the Pico 2 firmware target. Empty for now.
examples/   Sample text-format FOCAL programs (see "Program text format").
DEVIATIONS.md  Power-of-10 rule deviations: which rule, what's excepted,
               why, and the boundary of the exception. Same format/spirit
               as soynut's own DEVIATIONS.md.
```

### `core/` file responsibilities

- `vger_config.h` — every bounded loop and fixed-size buffer's constant.
  If you need a new bound anywhere in `core/`, it goes here, named, with a
  doc comment explaining what it bounds.
- `vger_assert.h` / `.c` — `VGER_ASSERT()`. Not `<assert.h>`: CMake's
  default Release build type defines `-DNDEBUG`, which silently compiles
  `assert()` to nothing. `VGER_ASSERT()` is independent of `NDEBUG`
  entirely (see DEVIATIONS.md), so it's always active regardless of build
  type. Use `VGER_ASSERT()`, never bare `assert()`, anywhere in this repo.
- `vger_types.h` — shared value types used by the public API:
  `vger_register_t` (tagged union: numeric double, or 6 packed alpha
  bytes), `vger_angle_mode_t`, `vger_display_format_t`, `vger_calc_mode_t`,
  `vger_annunciator_state_t`, `vger_key_id_t` (the core's entire key
  vocabulary — deliberately excludes MENU), `vger_key_event_t`.
- `vger_state_internal.h` — the concrete `struct vger_state` definition.
  **Included only by `vger_state.c` and `vger_interp.c`.** Nothing else in
  this repository may include it or reach into its fields. If you're
  writing display code, menu code, debugger code, or harness code and you
  think you need a field from here, you need a new query function in
  `vger_state.h` instead.
- `vger_state.h` / `.c` — the public query API (principle 1). Owns the
  single static `vger_state_t` instance (`vger_state_get()`) — there is
  exactly one calculator per running program, so this is a module-static
  instance, not heap-allocated (rule 3: no dynamic allocation, full stop,
  not just "after init"). Also owns `vger_state_reset()` (principle 5) and
  `vger_state_load_program()` (loads a `vger_program_t` — e.g. parsed from
  text — as an alternative to keystroke/PRGM-mode programming).
- `vger_program.h` / `.c` — the program IR (`vger_program_step_t`,
  `vger_program_t`) and the text-format parser
  (`vger_program_parse_text()`). See "Program text format" below.
- `vger_mode_policy.h` / `.c` — principle 3/4's swappable policy. See
  "Mode-boundary policy" below.
- `vger_interp.h` / `.c` — `vger_interp_handle_key()`, the core's entire
  mutation entry point besides `vger_state_reset()`. This is the biggest
  and most important file in `core/`; read its file-level comment before
  changing it. See "Interpreter semantics" below for the full behavior.

### `harness/` file responsibilities

- `main.c` — SDL2 init, event loop, keymap dispatch, seven-segment
  rendering, full-state terminal dump after every keystroke, optional
  program-file loading from `argv[1]`.
- `vger_sevenseg.h` / `.c` — a minimal seven-segment digit/`.`/`-`
  renderer. Deliberately not a general text/font renderer — see its file
  comment for why a full bitmap alphabet font was scoped out of milestone
  1 (the terminal dump covers ALPHA-buffer/register/flag content instead).
- `vger_keymap.h` / `.c` — maps SDL2 scancodes onto `vger_key_id_t` (core
  keys), and separately onto host-only actions (`VGER_HOST_ACTION_MENU`,
  `VGER_HOST_ACTION_RESET`) that never reach the core interpreter. See the
  keymap table below.

## Core data model

### Registers and stack

- Numeric registers/stack values are `double` (IEEE754), **not**
  BCD/decimal-faithful. This was a deliberate milestone-1 scope decision:
  documented-FOCAL program *logic* (branching, stack ops, flags) is
  provable without exact-decimal rounding/display fidelity. Revisit if a
  program's correctness starts to depend on exact HP-41 rounding behavior.
- Stack: `X`, `Y`, `Z`, `T`, plus `LASTX`. Storage registers: 00-99
  (`VGER_NUM_STORAGE_REGS`), each a tagged union (`vger_register_kind_t`):
  `VGER_REG_NUMERIC` (a double) or `VGER_REG_ALPHA_PACKED` (6 raw bytes,
  written by ASTO, read by ARCL).
- ALPHA buffer: 24 characters (`VGER_ALPHA_BUFFER_LEN`), matching the real
  HP-41.
- Flags: 00-99 array, but only 00-29 (`VGER_MAX_USER_FLAG`) are
  user-settable via SF/CF/FS?C in milestone 1; 30-99 are reserved and read
  back false. System-flag semantics (flags with side effects, like a
  "print mode" flag) are unimplemented.

### Stack lift

The stack-lift flag (`state->stack_lift_enabled`, internal-only) governs
whether the *next* value placed in X pushes the current X into Y first.
This is the real HP-41 RPN behavior, not a simplification:

- Starting a **fresh digit entry** (first digit/`.` key after a non-entry
  state) pushes iff lift is enabled, then leaves the flag unchanged.
- **ENTER^** pushes **unconditionally** (regardless of the flag — this is
  `vger_stack_push_unconditional()`, distinct from the conditional
  `vger_stack_push_lift()` used everywhere else), duplicates X into Y, X
  unchanged, then **disables** lift (so the next digit typed overwrites X
  instead of lifting again — this is what makes `5 ENTER 3` produce
  `Y=5 X=3`, not `Y=5 X=8`-via-double-push).
- **CLX** zeroes X (saving old X to LASTX) and disables lift.
- **Arithmetic** (`+ - * /`) consumes X and Y, drops the stack (Z→Y, T→Z,
  T duplicates), sets LASTX to the pre-op X, and **enables** lift
  afterward.
- **RCL** and **LASTX** (the key) push conditionally (respecting the
  current flag, like fresh digit entry) then enable lift afterward.
- **STO, ASTO, ARCL, CHS, X<>Y** don't touch the lift flag or push.

If you're debugging RPN "off by one push" behavior, this is the first
place to look — and `tests/test_core.c`'s `test_stack_lift_and_enter` is
the regression test that caught the original bug here (ENTER was using
the conditional push instead of the unconditional one).

### Live X during digit entry

`state->reg_x` is kept live during RUN-mode digit entry — updated on
*every* keystroke, not just when entry terminates. This matters because
real HP-41 hardware genuinely updates X character-by-character, and
because `vger_get_x()` is a documented query API consumers may read at any
time (principle 1) — it must reflect reality, not go stale until some
later flush. `vger_get_display_string()` separately reads the raw entry
buffer text (not the parsed double) while entry is in progress, so
in-progress formatting like trailing zeros or a bare trailing `.` displays
correctly even though the numeric value round-trips through `strtod()`.

In **PRGM mode**, digit entry does the opposite: it never touches
`reg_x`/the stack at all (recording a program must not mutate live
calculator state), and instead accumulates into the same buffer purely for
display, committing a `VGER_STEP_NUMBER_LITERAL` program step when entry
terminates. This RUN-vs-PRGM branch lives in
`vger_handle_digit_entry_key()` and `vger_flush_digit_entry()` — it's the
second bug the test suite caught (PRGM-mode number entry was corrupting
live registers instead of recording a step).

## Program representation (the IR) and text format

Programs are a flat, statically-sized array (`vger_program_t.steps`,
bounded by `VGER_PROGRAM_MAX_LINES` = 500) of `vger_program_step_t`, each
tagged by `vger_step_kind_t`:

| Kind | Meaning | Fields used |
|---|---|---|
| `VGER_STEP_NUMBER_LITERAL` | A bare number | `number_value` |
| `VGER_STEP_ALPHA_LITERAL` | A quoted alpha string | `alpha_value` |
| `VGER_STEP_OPCODE_ONLY` | ENTER^, `+ - * /`, CHS, X<>Y, CLX, LASTX, END, R/S, RTN, the 8 X=0?/X=Y?-family tests | `opcode` |
| `VGER_STEP_OPCODE_REG_ARG` | STO/RCL/ASTO/ARCL | `opcode`, `int_arg` = register 00-99 |
| `VGER_STEP_OPCODE_FLAG_ARG` | SF/CF/FS?C | `opcode`, `int_arg` = flag 00-29 |
| `VGER_STEP_OPCODE_LABEL_ARG` | LBL/GTO/XEQ | `opcode`, `int_arg` = label 00-99 |
| `VGER_STEP_OPCODE_DIGITS_ARG` | FIX/SCI/ENG | `opcode`, `int_arg` = digit count 0-9 |

This is **not** the real HP-41's packed byte-code, and there is no
byte-exact program-memory-size accounting. That's a deliberate scope cut
(see "Compatibility target" above): the IR proves parsing + execution +
the state API without taking on byte-exact memory modeling.

Two independent ways to get a program into a `vger_state_t`:

1. **Keystroke programming** (PRGM mode): press PRGM, then type
   instructions; each completed instruction is appended to
   `state->program` (see "PRGM mode is append-only" below). This is the
   authentic HP-41 workflow and the only path that exists on real hardware.
2. **Text format**, via `vger_program_parse_text()` (in `vger_program.c`)
   + `vger_state_load_program()` (in `vger_state.c`). One instruction per
   line, blank lines ignored. Mnemonics: `ENTER + - * / CHS X<>Y CLX
   LASTX END R/S RTN STO RCL ASTO ARCL SF CF FS?C LBL GTO XEQ FIX SCI ENG
   X=0? X#0? X>0? X<0? X=Y? X#Y? X>Y? X<Y?`,
   each opcode-with-argument mnemonic followed by a space and an integer
   (e.g. `STO 00`, `GTO 01`), or by `IND` and an integer for the indirect
   form (e.g. `STO IND 05`; see "Indirect addressing" below — every
   argument-taking mnemonic except LBL accepts this). A bare number on its
   own line is a number literal (e.g. `3.5`). A double-quoted string is an
   alpha literal (e.g. `"HELLO"`). See `examples/sum.focal` and
   `examples/subroutine.focal` for worked examples, and `harness/main.c`'s
   `vger_load_program_file()` for how the harness loads one from `argv[1]`.

Both paths converge on the same `vger_program_t`/`vger_execute_step()`
machinery — R/S execution doesn't know or care which path built the
program it's running.

## Interpreter semantics (`vger_interp.c`)

Read this section before touching `vger_interp_handle_key()` — it's the
one function whose behavior is easy to get subtly wrong without reading
the whole picture first.

### RUN vs. PRGM mode

- **RUN mode:** keys execute immediately against live state.
- **PRGM mode:** most keys are recorded as a new program step instead of
  executing. **Recording is append-only** — there is no mid-program
  insertion point, no navigating to an arbitrary line to edit/insert. This
  is a deliberate milestone-1 scope cut, not an oversight. Real HP-41 GTO
  has two forms (jump vs. navigate-to-insert); this project only
  implements the jump form. `VGER_KEY_PRGM` toggles between modes and
  flushes any pending digit entry first.
- **R/S always executes immediately, regardless of mode.** It runs the
  stored program **synchronously to completion** within one call to
  `vger_interp_handle_key()` — there is no live single-step/paused
  "running" state observable between keystrokes in milestone 1. This means
  `vger_get_is_running()` is essentially never observably true from
  outside the core in the current harness (the run starts and finishes
  within one function call). Real interruptible/steppable execution is
  deferred; see "Deferred work" below.

### The argument-collection state machine

Keys like STO, RCL, GTO, SF, LBL, FIX, etc. need a following 1-or-2-digit
argument. `vger_interp_handle_key()` handles this uniformly for both
RUN and PRGM mode via `state->awaiting_argument`:

1. Pressing the opcode key sets `awaiting_argument = true` and records
   which opcode (`awaiting_argument_for`) and how many digits it needs
   (looked up from the `VGER_ARG_SPECS` table in `vger_interp.c`: 2 digits
   for register/flag/label arguments, 1 digit for FIX/SCI/ENG's digit
   count).
2. Subsequent digit keys accumulate into `argument_digits` via
   `vger_handle_argument_digit_or_abort()`.
3. Once enough digits arrive, the completed instruction is built and
   passed to `vger_commit_instruction()`, which — this is the key
   RUN/PRGM unification — either **records** it as a program step (PRGM)
   or **executes** it immediately via `vger_execute_step()` (RUN).
4. **A non-digit key while an argument is pending cancels the pending
   instruction.** The interrupting key is *dropped*, not reprocessed —
   this is a deliberate simplification to avoid recursion at the top of
   the dispatcher (rule 1: no recursion). Press the interrupting key again
   if you meant it to do something.

### Execution: shared between direct keypress and R/S

`vger_execute_step()` is the single execution function used both when a
RUN-mode keypress commits an instruction immediately, and when
`vger_run_program()` (R/S) steps through stored program instructions. It
returns a `vger_exec_result_t { jumped, skip_next, halt }`:

- `jumped` — GTO, XEQ, or RTN repositioned `current_line` directly; the
  caller must not also auto-increment the line.
- `skip_next` — FS?C tested a *clear* flag (so it did nothing but signal
  "skip"); the run loop skips the following step. (If the flag was *set*,
  FS?C clears it and does **not** skip — matches real HP-41 FS?C
  semantics: "if set, clear it and continue; if clear, skip the next
  line.")
- `halt` — an END or R/S step was reached, RTN was reached with an empty
  call stack (treated the same as END), or XEQ overflowed the call stack
  (see "Subroutines" below) — the run loop stops.

### Subroutines (XEQ/RTN)

XEQ and RTN share `state->call_stack`, a fixed `VGER_CALL_STACK_MAX_DEPTH`
(8) array of return-line indices, manipulated only through
`vger_call_stack_push()`/`vger_call_stack_pop()` in `vger_interp.c`:

- **XEQ nn** looks up `LBL nn` exactly like GTO, but first pushes the
  *next* line (the one after the XEQ step) as a return address. If the
  stack is already full, XEQ **halts** rather than push past the bound —
  the same "refuse rather than corrupt state" posture as
  `VGER_MAX_STEPS_PER_RUN`, and exactly what `test_xeq_call_stack_overflow_halts`
  in `tests/test_core.c` exercises (a self-recursive `XEQ 01` with no base
  case runs out at depth 8, not a crash or a hang).
- **RTN** pops the most recent return address and jumps there. An RTN with
  an empty stack (reached at the top level, not inside any XEQ) is treated
  exactly like END — this is also what a bare RTN does if pressed as a
  direct RUN-mode keypress outside a running program.
- A fresh `vger_run_program()` call always starts with
  `call_stack_depth = 0` — a previous run's unfinished subroutine context
  (e.g. a program that hit END while still inside a called subroutine)
  never leaks into the next R/S press.
- `VGER_CALL_STACK_MAX_DEPTH` is a **named, deliberately chosen bound**,
  not a claimed hardware figure — real HP-41 subroutine nesting is
  memory-dependent, and this project doesn't model byte-exact program
  memory (see "Program representation" above). See its doc comment in
  `vger_config.h`.

`vger_run_program()` is bounded by `VGER_MAX_STEPS_PER_RUN` (100,000) —
documented FOCAL is Turing-complete, so no bound on "the algorithm" can be
proven, but rule 2 still requires an explicit bound, so a runaway/infinite
program becomes a detectable stop condition instead of a hang. This is the
one loop in `core/` whose bound isn't "this can't happen because the input
is finite" but "we refuse to run longer than this, on purpose."

### Indirect addressing (IND)

Every argument-taking instruction except LBL accepts an indirect form:
`STO IND 05` stores into whichever register register 05's *numeric
contents* name, not register 05 itself. This is a genuinely uniform
mechanism across STO/RCL/ASTO/ARCL (register-indirect), SF/CF/FS?C
(flag-indirect), GTO/XEQ (label-indirect), and FIX/SCI/ENG
(digit-count-indirect) — one resolution step, not four separate ones:

- `vger_program_step_t.indirect` marks a parsed/recorded step as
  indirect; `int_arg` then holds the *pointer register* (00-99), not the
  final argument.
- `vger_resolve_arg()` in `vger_interp.c` is the single place this gets
  resolved, called once per step from `vger_execute_step()` right before
  dispatching to whichever `vger_exec_*_arg()` helper handles that step's
  kind — those helpers are completely unaware indirect addressing exists;
  they just receive a resolved `int`, same as always. An out-of-range or
  non-numeric pointer register resolves to `-1`, which every helper
  already treats as out of range (they no-op on it), so no
  indirect-specific bounds checking exists anywhere but this one function.
- **Keystroke entry:** press `IND` right after the opcode key, before any
  digits — `state->awaiting_argument_indirect` then forces the following
  digit count to 2 regardless of the opcode's normal argument shape (e.g.
  `FIX IND 05` still needs 2 digits for register 05, even though a direct
  `FIX 5` only needs 1). Pressing `IND` with digits already typed, without
  a pending argument, or after LBL, is a no-op — LBL has no indirect form
  since its label number is a static marker matched by
  `vger_program_find_label()`'s literal scan, not something resolved at
  execution time.
- **Text format:** `MNEMONIC IND nn`, e.g. `STO IND 05`, `GTO IND 07`.

### Conditional-skip tests

`X=0? X#0? X>0? X<0?` and `X=Y? X#Y? X>Y? X<Y?` are the 8 documented
comparison instructions, non-destructive (X, Y, Z, T, LASTX all untouched)
and `VGER_STEP_OPCODE_ONLY` — no argument, so they need none of the
argument-collection machinery. FS?C's polarity carries over exactly: true
→ continue, false → `skip_next` (the run loop skips the following step).

`vger_exec_opcode_only()` delegates to `vger_exec_comparison()` for these
8 (checked via `vger_is_comparison_opcode()`) rather than growing its own
switch past ~60 lines (rule 4) — the same "split by category into a
sibling static helper" pattern as `vger_exec_reg_arg`/`_flag_arg`/
`_label_arg`/`_digits_arg`. Equality is tested exactly (`==`), matching
documented HP-41 behavior; this is the same exact-decimal-fidelity caveat
already noted for the IEEE754 `double` numeric model throughout (see
"Registers and stack" above), not a new one.

### Implemented instruction set (milestone 1)

Digit entry (0-9, `.`), CHS, ENTER^, `+ - * /`, X<>Y, CLX, LASTX, STO/RCL
(direct and indirect — see "Indirect addressing" below), GTO/LBL/END,
XEQ/RTN (subroutine calls, bounded call stack — see "Subroutines" below),
R/S, SF/CF/FS?C (flags 00-29), the 8 conditional-skip test instructions
(X=0?/X#0?/X>0?/X<0?, X=Y?/X#Y?/X>Y?/X<Y? — see "Conditional-skip tests"
below), ALPHA mode entry/backspace, ASTO/ARCL (6-char packing), FIX/SCI/
ENG. Every argument-taking instruction except LBL supports the indirect
(IND) form. ON (power toggle) and USER (annunciator-only stub, no
key-remapping behavior) round out the top-row keys.

**Not implemented, deferred:** matrix/complex data types, HP-IL/
printing, true ENG-format 3-digit-exponent grouping (`vger_state.c`
currently approximates ENG with plain SCI formatting — see the comment in
`vger_format_numeric_display()`), byte-exact program-memory accounting,
the full ~130-function HP-41 instruction set, and the MENU/system-menu
layer itself (principle 2: that's a future layer built on top of this
core, not part of it).

## Mode-boundary policy

`vger_mode_policy_may_enter_menu(policy, state)` decides whether a MENU
keypress should take effect right now. It's implemented as an enum
(`vger_mode_policy_id_t`) dispatched through a single `switch` in
`vger_mode_policy.c` — **deliberately not a function-pointer table**, even
though that's the more idiomatic C "policy interface" pattern. See
DEVIATIONS.md's rule-9 entry: `soynut`'s own conventions treat function
pointers in a project's *own* logic as never-exceptable (the only rule-9
exception there is calling into third-party SDK callback APIs), and this
project follows the same line. Adding a future policy (deferred entry,
always-live-with-snapshot/restore) means adding an enum value and a
`switch` case — that's what "swappable" means here, not runtime-pluggable
dispatch via function pointers.

Only `VGER_MODE_POLICY_IDLE_ONLY` exists: MENU takes effect iff the
calculator is powered on, not running a program, not mid-digit-entry, and
not mid-argument-entry (`vger_policy_idle_only()`).

MENU itself is never sent into the core (principle 2) — it's not a member
of `vger_key_id_t`. The harness's `vger_keymap_classify()` returns
`VGER_HOST_ACTION_MENU` for the Insert key, and `main.c`'s
`vger_handle_keydown()` calls the policy check directly and prints the
result; there is no actual menu system to enter yet (that's future work,
built on top of the core per principle 2, not inside it).

### MENU UI design constraints (informed by hands-on DB48X/C47 review)

Before the MENU/system-menu layer itself gets built (still future work —
see "Deferred work" below), two concrete constraints came out of using
DB48X (HP48/49/50 RPL) and C47 (HP-42S-lineage) firsthand, specifically to
see what to avoid:

1. **Entry point must stay dedicated and unshifted.** Both DB48X and C47
   bury menu/system access behind a shift-modified secondary or tertiary
   key function — in C47's case, literally described in its own docs as
   "the infamous cycling shift," a workaround for the DM42 having only one
   physical shift key. vger already avoids this *by construction*:
   principle 2's dedicated 5th key (replacing the old overlay-latch
   position) is a first-class entry point at the same level as ON/USER/
   PRGM/ALPHA, never a shifted function. This constraint is already
   satisfied by the existing hardware design — nothing to change, just
   don't compromise it later (e.g. by collapsing MENU onto a shift-layer
   of an existing key to save a physical key during firmware bring-up).
2. **Every menu transition needs visible feedback, governed by one
   consistent motion system — not implemented yet.** Both reference
   systems flash instantly from one menu screen to the next on selection,
   with no acknowledgment of what was just pressed and no persistent
   sense of where you are in the menu tree relative to where you were.
   The fix isn't "add some animations" bolted on per-screen — it's the
   same move Material Design makes: define one small internal "physics
   model" (easing curves, continuity rules, a fixed gesture vocabulary)
   that governs *every* transition in the system uniformly, so the user
   builds one intuition that transfers everywhere instead of re-learning
   each screen. Concretely, whatever renders the eventual menu layer
   needs:
   - **Selection feedback** — the pressed softkey should visibly
     acknowledge the press (e.g. briefly highlight/invert its label)
     before anything transitions, so cause and effect are connected.
   - **Positional feedback** — some persistent indication of depth/path
     in the menu tree (a breadcrumb, a path label, consistent per-depth
     framing) rather than each screen reading as an unrelated flash-cut.
   This isn't just a taste preference: vger's target display (the
   NHD14432, ST7920 controller — see "NHD14432/ST7920 driver" above) has
   its own onboard GDRAM, so it doesn't carry the Sharp Memory LCD's
   VCOM-refresh/panel-degradation risk this section used to cite here —
   but writing only the bytes that actually changed over the 8-bit
   parallel bus is still cheaper than redrawing the full 144×32 frame on
   every transition, so the same "don't flash-cut the whole panel"
   motivation still holds, just for plain bus-bandwidth reasons now
   rather than a panel-degradation one. A deliberate, visible transition
   is motivated on both UX and bus-bandwidth fronts.

   **Reference for what this motion system could look like:** Peng
   Zhihui's (稚晖君's) MonoUI — an animation framework for monochrome
   displays (OLED/VFD/e-Paper) licensed exclusively to Xikii for their
   UltraLink product, never open-sourced; only demo video exists, no code
   to read. [WouoUI](https://github.com/RQNG/WouoUI) is an unlicensed
   (all-rights-reserved by default — same reference-only treatment as
   Nonpareil/DB48X/C47 above, not a source to vendor) open tribute that
   documents the actual technique in enough detail to be genuinely
   useful as a study reference:
   - Non-linear easing applied *uniformly* — lists, popups, even progress
     bars — not just the primary selection indicator.
   - **Interruptible, blending transitions**: triggering a new transition
     before the current one finishes blends into the new target instead
     of snapping/resetting. Likely the single biggest contributor to
     "smooth" vs. DB48X/C47's instant flash-cuts.
   - **Positional continuity** — the direct technique for the
     "positional feedback" requirement above: a selection indicator's
     size and position animate back to wherever it was last selected
     when navigating back up a level, rather than resetting to the top.
   - **One small, fixed gesture vocabulary reused at every depth** (in
     WouoUI's rotary-encoder case: rotate to move selection, short-press
     to select, long-press to go back up one level, every level) — the
     same "one physics model everywhere" principle applied to input, not
     just rendering. vger's own input model differs (4 shifted keys +
     dedicated MENU, not a rotary encoder), but the principle — a fixed,
     depth-independent gesture set — carries over directly.

## Coding conventions

Applies to this project's own original code (everything in `core/`,
`harness/`, `tests/`, and eventually `firmware/`'s non-vendored files) —
not to any third-party/vendored code that gets pulled in later.

### NASA/JPL "Power of 10" rules — strong defaults, documented exceptions

Full rule list and rationale: the project kickoff conversation (not
reproduced here); the practical upshot per rule, as actually applied in
this codebase:

1. **No `goto`/`setjmp`/`longjmp`, no recursion.** Nothing in `core/` or
   `harness/` recurses. The text parser (`vger_program_parse_text()`) is
   line-based, not a recursive-descent expression parser, specifically so
   it never needs to recurse.
2. **Every loop has a provable, named upper bound.** Every bound traces to
   a named constant in `vger_config.h` (`VGER_PROGRAM_MAX_LINES`,
   `VGER_MAX_STEPS_PER_RUN`, etc.) or a harness-local constant
   (`VGER_MAX_EVENTS_PER_FRAME`, `VGER_DISPLAY_MAX_CHARS`). Two
   *justified* exceptions exist (an interactive event loop that runs until
   the user quits has no other bound) — see DEVIATIONS.md for both
   (`harness/main.c`'s top-level `while (running)`, matching the same
   class of exception `soynut` documents for its own firmware).
3. **No dynamic allocation, ever** (not just "after init" — this project
   has none at all). The single `vger_state_t` is a module-static instance
   (`vger_state_get()`'s doc comment explains why one instance is the
   correct model — one calculator per running program). All buffers are
   fixed-size, stack- or statically-allocated.
4. **~60 lines per function.** Where an operation naturally splits by
   category (e.g. instruction execution), it's split into per-category
   static helpers (`vger_exec_opcode_only`, `vger_exec_reg_arg`,
   `vger_exec_flag_arg`, `vger_exec_label_arg`, `vger_exec_digits_arg` in
   `vger_interp.c`) rather than one large `switch`.
5. **At least 2 assertions per function, via `VGER_ASSERT()`, not bare
   `assert()`.** See `vger_assert.h`'s doc comment and the DEVIATIONS.md
   entry on why this project rolled its own instead of using `-UNDEBUG`
   per-target the way `soynut` does.
6. **Smallest possible variable scope.** The one deliberate global is the
   single state instance, module-static (not `extern`, not
   consumer-visible) inside `vger_state.c`.
7. **Check every non-void return value; validate every parameter.** Every
   public function in `core/` starts with `VGER_ASSERT()` parameter checks.
8. **Preprocessor limited to header inclusion + simple macros.** No
   conditional-compilation tricks anywhere in this codebase yet. When
   `firmware/` starts existing and needs platform `#ifdef`s, keep them
   minimal and scoped, following `soynut`'s precedent of treating any
   necessary exception as narrowly as possible and writing it down.
9. **One level of pointer dereference; no function pointers in this
   project's own logic.** See "Mode-boundary policy" above — this is the
   rule that most directly shaped a design decision here, not just a
   background constraint.
10. **Pedantic warnings, zero warnings, static analysis wired into the
    build.** See "Build & lint setup" below — this is enforced by
    `CMakeLists.txt`, not aspirational.

Deviations (all of them, with the "why" and the exact boundary) live in
`DEVIATIONS.md`. Read it before assuming a rule is unconditionally
followed everywhere — and if you find a new place where a rule is being
silently ignored rather than followed or explicitly excepted, fix it or
add it there; don't leave it undocumented.

### Commenting standard

**C (all of it, so far):** Doxygen-style `/** ... */`. Every file has a
`@file`/`@brief` header. Every function — including `static` helpers —
has a `/** @brief ... */` block; add `@param`/`@return` for anything
non-trivial. Non-obvious structs/typedefs/`#define`s get their own doc
block. Inline `//`/`/* */` comments cover non-obvious in-body logic on top
of that, not instead of it. Default to no comment at all when a
well-named identifier already says what's happening — comments earn their
place by explaining *why*, not *what*.

No Python or generated files exist in this repository yet. If either
shows up: Python gets Google-style docstrings enforced via `ruff` (`select
= ["ALL"]`, pydocstyle convention `"google"`), using a `check(condition,
message)` helper instead of bare `assert` (Python's `assert` is stripped
under `-O`/`-OO`, the same footgun `VGER_ASSERT()` avoids in C — see
`soynut`'s equivalent convention). Generated files get a "do not hand-edit"
header and the generator script itself gets documented, not the output.

### Build & lint setup

`CMakeLists.txt` (top level) defines `VGER_STRICT_WARNING_FLAGS`
(`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion
-Wundef -Wcast-qual -Wpointer-arith -Wwrite-strings -Wredundant-decls
-Wmissing-prototypes -Wstrict-prototypes`) and applies them via
`set_source_files_properties(<this target's own source list> PROPERTIES
COMPILE_OPTIONS ...)` in each subdirectory's `CMakeLists.txt` — **scoped
to a named file list, never target-wide.** This is deliberate, not
incidental: `soynut` discovered that target-wide strict flags leak onto
vendored/SDK sources compiled into the same target (broke on
`pico-sdk`'s own `bootrom.c`) or clobber flags already set on those files.
There's no vendored code in this build yet (SDL2 is linked as a system
library, not compiled from vendored source), so the leakage risk is
currently theoretical — but the scoping is already set up correctly so
adding `firmware/` and `pico-sdk` later doesn't require retrofitting it.

`clang-tidy` is wired in automatically (`CMAKE_C_CLANG_TIDY`) if found on
`PATH` via `find_program`; if not found, CMake prints a status message
saying so rather than silently skipping it. Install via `brew install
llvm` (macOS) to activate it.

`VGER_ENABLE_STRICT_WARNINGS` (CMake option, default `ON`) can disable all
of the above; there's no reason to, but it exists for bisection/debugging.

`.github/workflows/ci.yml` runs this on every push/PR to `main`, on both
Ubuntu and macOS: a full build+`ctest` run (harness included, with SDL2 and
`clang-tidy` installed on the runner so both are actually exercised, not
just locally optional), plus a second `-DVGER_BUILD_HARNESS=OFF` build to
keep `core/`+`tests/`'s "zero dependencies beyond a C17 compiler" claim
honest. Since `-Werror` is already on, a warning fails CI the same way a
failing test does — there's no separate lint-only step.

## Building and testing

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # desktop unit tests (core only)
./build/harness/vger_harness                 # interactive desktop harness
./build/harness/vger_harness examples/sum.focal  # load a program at startup
```

Requires SDL2 (`brew install sdl2` on macOS) for `harness/` only.
`core/` and `tests/` have zero dependencies beyond a C17 compiler and can
be built alone with `-DVGER_BUILD_HARNESS=OFF`. `-DVGER_BUILD_FIRMWARE=ON`
exists as an option but `firmware/` doesn't have a `CMakeLists.txt` yet —
don't turn it on until that milestone starts.

`tests/test_core.c` drives the interpreter through synthetic
`vger_key_event_t` sequences and checks results via the public query API
— the same pattern any future test should follow. It caught two real bugs
during development (see "Stack lift" and "Live X during digit entry"
above); if you change interpreter semantics, run `ctest` before assuming
it still works.

## Desktop harness keymap

Printed at harness startup too (`vger_print_keymap_legend()` in
`main.c`), reproduced here for reference:

| Host key | Core action |
|---|---|
| `0`-`9`, `.` (also numpad) | digit entry |
| Enter / numpad Enter | ENTER^ |
| Backspace | backspace / cancel last alpha char |
| Numpad `+ - * /` | arithmetic |
| `N` | CHS |
| Tab | X<>Y |
| Delete | CLX |
| `L` | LASTX |
| `I` | IND (indirect prefix; press right after an opcode key, before its digits — no effect after LBL) |
| F1 / F2 / F3 / F4 | STO / RCL / GTO / LBL (each followed by 2 digits) |
| F5 / F6 / F7 | SF / CF / FS?C (each followed by 2 digits) |
| `Q` `W` `E` `T` | X=0? / X#0? / X>0? / X<0? |
| `A` `S` `D` `F` | X=Y? / X#Y? / X>Y? / X<Y? |
| F8 / F9 | ASTO / ARCL (each followed by 2 digits) |
| F10 / `,` / `;` | FIX / SCI / ENG (each followed by 1 digit) |
| `X` | XEQ (followed by 2 digits) |
| `R` | RTN |
| Space | R/S |
| `P` | PRGM (toggle) |
| `U` | USER (toggle; annunciator only) |
| Home | ON (power toggle) |
| Escape | ALPHA (toggle) |
| Insert | MENU (5th key; gated by the mode policy, stubbed) |
| F12 | RESET (out-of-band; see principle 5) |
| window close | quit |

While ALPHA mode is active, letter/digit/space/period keys are routed to
`VGER_KEY_ALPHA_CHAR` (typing literal text into the ALPHA buffer) instead
of their normal function meaning — this is why function-key bindings were
chosen over letter keys for STO/RCL/GTO/etc. in the first place, to avoid
ambiguity with alpha text entry. See `vger_keymap_classify()`'s doc
comment for the exact precedence rules.

## Milestone status

**Milestone 1: done.** FOCAL interpreter core with the full state/query
API, swappable mode-boundary policy (idle-only implemented), out-of-band
reset, bounded documented-FOCAL instruction subset, desktop SDL2 test
harness, desktop unit test suite. All architecture principles proven
end-to-end without any hardware dependency.

**Since milestone 1:** XEQ/RTN subroutine calls (bounded call stack, see
"Subroutines" above), CI (GitHub Actions, Ubuntu + macOS,
`.github/workflows/ci.yml`), indirect addressing (IND, see "Indirect
addressing" above), the 8 conditional-skip test instructions (see
"Conditional-skip tests" above).

**Deferred work, in rough likely order:**
1. True ENG-format display, BCD-faithful numeric semantics if a program's
   correctness turns out to depend on it.
2. The MENU/system-menu layer itself (principle 2: on top of the core;
   see "MENU UI design constraints" above for two concrete requirements
   to design against before/while building it).
3. Pico 2 firmware bring-up: NHD14432/ST7920 driver (port from `soynut`,
   see "NHD14432/ST7920 driver" above; the desktop harness's logical
   canvas and seven-segment renderer already match 144×32, per "Hardware
   target" above), 5-key matrix input, physical reset switch —
   `firmware/` is reserved and empty for this.

Do not start on 3 before the earlier items are either done or explicitly
decided to be skipped — the whole point of milestone 1 was proving the
core architecture before hardware bring-up begins.

## License

Apache License 2.0 (see `LICENSE`, `NOTICE`). Applies to this project's
own original code. Nonpareil (GPLv2) was consulted only as a reference
for understanding documented FOCAL behavior — no Nonpareil code is
vendored or adapted into this repository (see "Compatibility target"
above). If a vendored third-party dependency is ever added (e.g. when
`firmware/` starts pulling in `pico-sdk`), its own license applies to it
and that should be noted here and in `NOTICE` when it happens.
