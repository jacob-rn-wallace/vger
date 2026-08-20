/**
 * @file vger_program.h
 * @brief FOCAL program representation (AST/IR) and text-format parser.
 *
 * Milestone 1 stores programs as a flat array of typed steps rather than
 * the real HP-41's packed byte-code, and authors them from a simple
 * human-readable text format (one instruction per line) rather than
 * keystroke-by-keystroke recording. This proves parsing + execution + the
 * state/query API without taking on byte-exact program-memory accounting,
 * which is out of scope for documented-FOCAL compatibility.
 */

#ifndef VGER_PROGRAM_H
#define VGER_PROGRAM_H

#include <stdbool.h>
#include <stddef.h>

#include "vger_config.h"
#include "vger_types.h"

/** @brief Shape of one parsed program step; which fields of
 *  vger_program_step_t are meaningful depends on this tag. */
typedef enum {
    VGER_STEP_NUMBER_LITERAL,   /**< A bare number, e.g. digit-entry recorded as a step. */
    VGER_STEP_ALPHA_LITERAL,    /**< A quoted alpha string, e.g. "HELLO". */
    VGER_STEP_OPCODE_ONLY,      /**< ENTER, +, -, *, /, CHS, X<>Y, CLX, LASTX, END, R/S. */
    VGER_STEP_OPCODE_REG_ARG,   /**< STO/RCL/ASTO/ARCL: int_arg is register 00-99. */
    VGER_STEP_OPCODE_FLAG_ARG,  /**< SF/CF/FS?C: int_arg is flag 00-29. */
    VGER_STEP_OPCODE_LABEL_ARG, /**< LBL/GTO: int_arg is label number 00-99. */
    VGER_STEP_OPCODE_DIGITS_ARG /**< FIX/SCI/ENG: int_arg is digit count 0-9. */
} vger_step_kind_t;

/** @brief One parsed, executable program step. */
typedef struct {
    vger_step_kind_t kind;
    vger_key_id_t opcode; /**< Meaningful for all kinds except the two literal kinds. */
    double number_value; /**< Valid iff kind == VGER_STEP_NUMBER_LITERAL. */
    char alpha_value[VGER_ALPHA_BUFFER_LEN]; /**< Valid iff kind == VGER_STEP_ALPHA_LITERAL; NUL-terminated. */
    int int_arg; /**< Valid for the *_ARG kinds. */
    bool indirect; /**< Valid for the *_ARG kinds except LBL: if true,
                     *   int_arg names a storage register whose (truncated)
                     *   numeric contents are the real argument, resolved at
                     *   execution time - not int_arg itself. */
} vger_program_step_t;

/** @brief A full program: a bounded, statically-sized array of steps. */
typedef struct {
    vger_program_step_t steps[VGER_PROGRAM_MAX_LINES];
    int step_count;
} vger_program_t;

/** @brief Reset a program to empty (step_count = 0). */
void vger_program_clear(vger_program_t *program);

/**
 * @brief Parse a text-format FOCAL program into program, replacing any
 * existing contents.
 *
 * Any argument-taking mnemonic except LBL accepts an indirect form:
 * `STO IND 05` stores into whatever register 05's numeric contents name,
 * rather than register 05 itself (see vger_program_step_t.indirect).
 *
 * @param text NUL-terminated source text, one instruction per line.
 * @param program Destination; cleared before parsing, untouched on failure.
 * @param err_buf Optional buffer to receive a human-readable error message.
 * @param err_buf_len Size of err_buf, including NUL; ignored if err_buf is NULL.
 * @return true on success, false if a line could not be parsed (in which
 *         case *program is left cleared).
 */
bool vger_program_parse_text(const char *text, vger_program_t *program, char *err_buf, size_t err_buf_len);

/**
 * @brief Find the array index of a LBL step with the given label number.
 *
 * @param program Program to search.
 * @param label_number Label number to look for (00-99).
 * @param out_index Set to the matching step's index on success.
 * @return true if found, false otherwise (out_index left unmodified).
 */
bool vger_program_find_label(const vger_program_t *program, int label_number, int *out_index);

#endif /* VGER_PROGRAM_H */
