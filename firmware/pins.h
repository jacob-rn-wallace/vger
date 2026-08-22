/**
 * @file pins.h
 * @brief GPIO pin assignments for the NHD14432/ST7920 parallel LCD link.
 *
 * Ported from `soynut`'s `firmware/pins.h` (same author, same physical
 * display part - see DEVIATIONS.md for the license note this port
 * crosses). vger's own hardware doesn't exist yet (see CLAUDE.md's
 * "Hardware target"), so this carries over soynut's already
 * hardware-validated pin assignment and wiring plan as the starting
 * point rather than inventing a new one - see `soynut/CLAUDE.md`'s
 * "Direct Pico->LCD parallel link" section for the full validation
 * history (level-shifter board grouping, confirmed working via solid-fill
 * and checkerboard test patterns).
 */
#ifndef VGER_PINS_H
#define VGER_PINS_H

// NHD-14432WG-BTFH-VT, ST7920 controller, wired 8-bit parallel - the
// LCD board's own default interface (no jumper change needed).
//
// Full 16-pin connector, parallel pinout per the datasheet
// ("Pin Description - Parallel Interface"):
//   1  VSS      -> GND
//   2  VDD      -> +5V (NOT 3.3V - see level-shifting note below)
//   3  VO       -> no connect (fixed-contrast variant, no pot needed)
//   4  RS       -> level shifter -> VGER_PIN_LCD_RS below
//   5  R/W      -> tied DIRECTLY to GND (write-only design, no busy-flag
//                  reads). A constant 0V signal on both voltage domains,
//                  so it does NOT go through a level shifter channel -
//                  just a wire straight from LCD pin 5 to GND.
//   6  E        -> level shifter -> VGER_PIN_LCD_E below (FALLING-EDGE
//                  triggered - confirmed from the datasheet's own pin
//                  table and 8051 reference code: set RS + data bus,
//                  raise E, brief delay, drop E to latch)
//   7-14 DB0-DB7 -> level shifter -> VGER_PIN_LCD_DB0-7 below
//   15 LED+, 16 LED- -> no connect (no backlight, by design)
//
// *** LEVEL SHIFTING ***
// This module's VDD is 5V and its logic-high input threshold is
// 0.7*VDD = 3.5V minimum - above the Pico's ~3.3V GPIO output high. 10
// signals need shifting (RS, E, DB0-7): three 4-channel bidirectional
// auto-sensing level-shifter boards (12 channels total, 2 spare) cover
// it, grouped to line up with the sequential GP0-9 assignment below:
//   Board A ch1-4 -> RS, E, DB0, DB1    (GP0, GP1, GP2, GP3)
//   Board B ch1-4 -> DB2, DB3, DB4, DB5 (GP4, GP5, GP6, GP7)
//   Board C ch1-2 -> DB6, DB7           (GP8, GP9) - ch3/ch4 spare
// Every board's low side (3.3V/GND) ties to the Pico's 3V3 and GND;
// every board's high side (5V/GND) ties to the same 5V rail as LCD VDD
// and GND. All grounds must land in one common net - the auto-sensing
// bidirectional shifter circuit needs a shared ground reference between
// voltage domains to work at all.

#define VGER_PIN_LCD_RS   0
#define VGER_PIN_LCD_E    1
#define VGER_PIN_LCD_DB0  2
#define VGER_PIN_LCD_DB1  3
#define VGER_PIN_LCD_DB2  4
#define VGER_PIN_LCD_DB3  5
#define VGER_PIN_LCD_DB4  6
#define VGER_PIN_LCD_DB5  7
#define VGER_PIN_LCD_DB6  8
#define VGER_PIN_LCD_DB7  9

#endif // VGER_PINS_H
