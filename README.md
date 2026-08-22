# vger

[![CI](https://github.com/jacob-rn-wallace/vger/actions/workflows/ci.yml/badge.svg)](https://github.com/jacob-rn-wallace/vger/actions/workflows/ci.yml)

An HP-41-inspired native calculator system — not a hardware emulator.
`vger` targets documented FOCAL behavior (the standard, published
instruction/function set) on modern hardware, not byte-exact real-ROM
execution, third-party accessory ROMs, or synthetic programming. See
`DEVIATIONS.md` for where this project's code deliberately departs from
its own coding conventions, and why. See `CLAUDE.md` for the full
architecture/conventions reference this README summarizes.

Named for V'Ger, from *Star Trek: The Motion Picture* — a simple probe
transformed into something far beyond its original design, which is the
spirit of taking the HP-41's simple interface further than the original
hardware ever could. Continues HP's own space-probe calculator naming
(Voyager, Pioneer, Saturn) and this author's own convention of naming
calculator projects after space probes — `Cassini` (a separate project, an
HP Saturn-CPU-family replica) being the other one so far. `soynut` (an
HP-41CV replica, unrelated name) is a different related project this
one's architecture conventions are informed by, and now also its hardware
display target.

**Conceptual model:** an HP-28C's capability (a real menu system, even
graphing) delivered through an HP-41C's keyboard, with the top row
replaced by 5 soft keys. The HP-28C's own display was 137×32 dot-matrix —
almost identical to this project's 144×32 target — and shipped a full
menu system and graphing at that resolution, so the target is a proven
constraint, not a hopeful one. See `CLAUDE.md` for the full reasoning.

## Layout

```
core/       FOCAL interpreter core: state/query API, mode-boundary policy,
            key dispatch, program parser. Zero hardware/SDK dependencies -
            builds and runs identically on desktop and (later) Pico 2.
harness/    Desktop SDL2 test harness (milestone 1's proof-of-architecture
            target). Renders the display line as seven-segment digits in a
            144x32 canvas matching the real NHD14432 hardware target, with
            digit/annunciator geometry borrowed from soynut's own measured
            layout for that same display (see CLAUDE.md), and dumps full
            register/flag/alpha state to the terminal after every
            keystroke.
tests/      Desktop unit tests exercising the core directly through
            synthetic key events - no display/keypad hardware needed.
firmware/   Pico 2 firmware target (later milestone; not yet populated).
examples/   Sample text-format FOCAL programs.
```

## Architecture

1. **State/query API is the only way in.** `core/vger_state.h` is the
   sole public surface for observing calculator state (registers, stack,
   alpha buffer, flags, annunciators). `vger_state_t` is opaque outside
   `core/`; the concrete struct lives in `vger_state_internal.h`, included
   only by `vger_state.c` and `vger_interp.c`.
2. **Native-41 interaction and any extended/menu layer are separate.**
   The core's key vocabulary (`vger_key_id_t`) deliberately excludes MENU;
   a host input loop intercepts it before the core ever sees it.
3. **The mode-boundary policy is swappable from day one.**
   `vger_mode_policy_may_enter_menu()` dispatches on an enum
   (`vger_mode_policy_id_t`), not a function-pointer table — see
   `DEVIATIONS.md`'s rule-9 entry for why. Adding a policy means adding an
   enum value and a switch case.
4. **Only the idle-only policy is implemented.** MENU only takes effect
   when nothing is mid-digit-entry, mid-argument-entry, or running.
5. **Reset is out-of-band.** `vger_state_reset()` is a plain function call,
   not a key in `vger_key_id_t` — it can never be reached through
   `vger_interp_handle_key()`'s normal dispatch. The harness wires it to a
   dedicated key (F12) outside the regular keymap table.

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # desktop unit tests
./build/harness/vger_harness                 # interactive desktop harness
./build/harness/vger_harness examples/sum.focal  # load a program at startup
```

Requires SDL2 (`brew install sdl2` on macOS) for the harness; `core/` and
`tests/` have no dependencies beyond a C17 compiler. Set
`-DVGER_BUILD_HARNESS=OFF` to build `core/`+`tests/` alone.

If `clang-tidy` is on `PATH` (e.g. `brew install llvm`), it's wired into
the build automatically.

## Scope

Implements a bounded, documented-FOCAL subset: digit entry, ENTER^/CLX/
X<>Y/LASTX, `+ - * /`/CHS, STO/RCL, GTO/LBL/END, XEQ/RTN subroutine calls
(bounded call stack), indirect addressing (IND — every argument-taking
instruction except LBL), R/S execution, SF/CF/FS?C on flags 00-29, the 8
conditional-skip tests (X=0?/X#0?/X>0?/X<0?, X=Y?/X#Y?/X>Y?/X<Y?), ALPHA
entry with ASTO/ARCL packing, and FIX/SCI/ENG. See `core/vger_interp.c`'s
file comment for the specific simplifications this implies (append-only
PRGM recording, synchronous R/S, non-digit-key-cancels-pending-argument).
Explicitly deferred: matrix/complex data, BCD-faithful numeric semantics,
byte-exact program memory accounting, the MENU/system-menu layer itself,
and all Pico 2/display/keypad hardware bring-up.
