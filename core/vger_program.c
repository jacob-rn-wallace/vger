/**
 * @file vger_program.c
 * @brief Implementation of the FOCAL program IR and text-format parser.
 */

#include "vger_program.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vger_assert.h"

/** @brief Maximum characters accepted on one source line, including NUL. */
#define VGER_PARSE_LINE_LEN 64

/** @brief One entry in the mnemonic lookup table. */
typedef struct {
    const char *name;
    vger_key_id_t opcode;
    vger_step_kind_t kind;
} vger_mnemonic_entry_t;

/** @brief All opcode-form mnemonics the milestone-1 parser understands. */
static const vger_mnemonic_entry_t VGER_MNEMONICS[] = {
    {"ENTER", VGER_KEY_ENTER, VGER_STEP_OPCODE_ONLY},
    {"+", VGER_KEY_PLUS, VGER_STEP_OPCODE_ONLY},
    {"-", VGER_KEY_MINUS, VGER_STEP_OPCODE_ONLY},
    {"*", VGER_KEY_TIMES, VGER_STEP_OPCODE_ONLY},
    {"/", VGER_KEY_DIVIDE, VGER_STEP_OPCODE_ONLY},
    {"CHS", VGER_KEY_CHS, VGER_STEP_OPCODE_ONLY},
    {"X<>Y", VGER_KEY_X_EXCHANGE_Y, VGER_STEP_OPCODE_ONLY},
    {"CLX", VGER_KEY_CLX, VGER_STEP_OPCODE_ONLY},
    {"LASTX", VGER_KEY_LASTX, VGER_STEP_OPCODE_ONLY},
    {"END", VGER_KEY_END, VGER_STEP_OPCODE_ONLY},
    {"R/S", VGER_KEY_RUN_STOP, VGER_STEP_OPCODE_ONLY},
    {"STO", VGER_KEY_STO, VGER_STEP_OPCODE_REG_ARG},
    {"RCL", VGER_KEY_RCL, VGER_STEP_OPCODE_REG_ARG},
    {"ASTO", VGER_KEY_ASTO, VGER_STEP_OPCODE_REG_ARG},
    {"ARCL", VGER_KEY_ARCL, VGER_STEP_OPCODE_REG_ARG},
    {"SF", VGER_KEY_SF, VGER_STEP_OPCODE_FLAG_ARG},
    {"CF", VGER_KEY_CF, VGER_STEP_OPCODE_FLAG_ARG},
    {"FS?C", VGER_KEY_FS_QUESTION_C, VGER_STEP_OPCODE_FLAG_ARG},
    {"LBL", VGER_KEY_LBL, VGER_STEP_OPCODE_LABEL_ARG},
    {"GTO", VGER_KEY_GTO, VGER_STEP_OPCODE_LABEL_ARG},
    {"FIX", VGER_KEY_FIX, VGER_STEP_OPCODE_DIGITS_ARG},
    {"SCI", VGER_KEY_SCI, VGER_STEP_OPCODE_DIGITS_ARG},
    {"ENG", VGER_KEY_ENG, VGER_STEP_OPCODE_DIGITS_ARG},
};

#define VGER_MNEMONIC_COUNT (sizeof(VGER_MNEMONICS) / sizeof(VGER_MNEMONICS[0]))

void vger_program_clear(vger_program_t *program)
{
    VGER_ASSERT(program != NULL);
    program->step_count = 0;
    VGER_ASSERT(program->step_count == 0);
}

/** @brief Copy one line (without its terminator) from *cursor into out,
 *  advancing *cursor past the terminator. Bounded by out_len.
 *
 * @return Number of characters copied (may be 0 for a blank line). */
static size_t vger_extract_line(const char **cursor, char *out, size_t out_len)
{
    VGER_ASSERT(cursor != NULL && *cursor != NULL);
    VGER_ASSERT(out != NULL && out_len > 0);

    size_t len = 0;
    const char *p = *cursor;

    while (*p != '\0' && *p != '\n' && len + 1U < out_len) {
        out[len] = *p;
        len++;
        p++;
    }
    out[len] = '\0';

    while (*p != '\0' && *p != '\n') {
        p++; /* skip any overflow tail of an over-long line */
    }
    if (*p == '\n') {
        p++;
    }
    *cursor = p;
    return len;
}

/** @brief Strip leading/trailing whitespace from a NUL-terminated buffer
 *  in place. */
static void vger_trim(char *text)
{
    VGER_ASSERT(text != NULL);

    size_t len = strlen(text);
    size_t start = 0;
    while (start < len && isspace((unsigned char)text[start])) {
        start++;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end--;
    }
    size_t out_len = end - start;
    if (start > 0) {
        memmove(text, text + start, out_len);
    }
    text[out_len] = '\0';
    VGER_ASSERT(out_len <= len);
}

/** @brief Look up a mnemonic token in VGER_MNEMONICS.
 *  @return true and fills *entry on match, false otherwise. */
static bool vger_lookup_mnemonic(const char *token, vger_mnemonic_entry_t *entry)
{
    VGER_ASSERT(token != NULL);
    VGER_ASSERT(entry != NULL);

    for (size_t i = 0; i < VGER_MNEMONIC_COUNT; i++) {
        if (strcmp(token, VGER_MNEMONICS[i].name) == 0) {
            *entry = VGER_MNEMONICS[i];
            return true;
        }
    }
    return false;
}

/** @brief Parse one already-trimmed, non-empty line into out_step.
 *  @return true on success, false with *err set on failure. */
static bool vger_parse_line(const char *line, vger_program_step_t *out_step, const char **err)
{
    VGER_ASSERT(line != NULL);
    VGER_ASSERT(out_step != NULL);
    memset(out_step, 0, sizeof(*out_step));

    size_t line_len = strlen(line);
    if (line[0] == '"') {
        if (line_len < 2 || line[line_len - 1] != '"') {
            *err = "unterminated alpha literal";
            return false;
        }
        size_t body_len = line_len - 2;
        if (body_len >= VGER_ALPHA_BUFFER_LEN) {
            *err = "alpha literal too long";
            return false;
        }
        out_step->kind = VGER_STEP_ALPHA_LITERAL;
        memcpy(out_step->alpha_value, line + 1, body_len);
        out_step->alpha_value[body_len] = '\0';
        return true;
    }

    if (isdigit((unsigned char)line[0]) || ((line[0] == '-' || line[0] == '.') && line_len > 1)) {
        char *end = NULL;
        double value = strtod(line, &end);
        if (end == line || *end != '\0') {
            *err = "malformed number literal";
            return false;
        }
        out_step->kind = VGER_STEP_NUMBER_LITERAL;
        out_step->number_value = value;
        return true;
    }

    char mnemonic[VGER_PARSE_LINE_LEN];
    char arg_text[VGER_PARSE_LINE_LEN];
    mnemonic[0] = '\0';
    arg_text[0] = '\0';
    int scanned = sscanf(line, "%31s %31s", mnemonic, arg_text);
    if (scanned < 1) {
        *err = "empty instruction";
        return false;
    }

    vger_mnemonic_entry_t entry;
    if (!vger_lookup_mnemonic(mnemonic, &entry)) {
        *err = "unrecognized mnemonic";
        return false;
    }

    out_step->kind = entry.kind;
    out_step->opcode = entry.opcode;

    bool needs_arg = (entry.kind != VGER_STEP_OPCODE_ONLY);
    if (needs_arg) {
        if (scanned < 2) {
            *err = "missing argument";
            return false;
        }
        char *end = NULL;
        long arg = strtol(arg_text, &end, 10);
        if (end == arg_text || *end != '\0') {
            *err = "malformed argument";
            return false;
        }
        out_step->int_arg = (int)arg;
    }

    return true;
}

bool vger_program_parse_text(const char *text, vger_program_t *program, char *err_buf, size_t err_buf_len)
{
    VGER_ASSERT(text != NULL);
    VGER_ASSERT(program != NULL);

    vger_program_clear(program);
    const char *cursor = text;
    int line_number = 0;

    for (int i = 0; i < VGER_PROGRAM_MAX_LINES; i++) {
        if (*cursor == '\0') {
            break;
        }
        line_number++;

        char raw_line[VGER_PARSE_LINE_LEN];
        (void)vger_extract_line(&cursor, raw_line, sizeof(raw_line));
        vger_trim(raw_line);
        if (raw_line[0] == '\0') {
            i--; /* blank lines don't consume a program step slot */
            continue;
        }

        vger_program_step_t step;
        const char *err = NULL;
        if (!vger_parse_line(raw_line, &step, &err)) {
            if (err_buf != NULL && err_buf_len > 0) {
                (void)snprintf(err_buf, err_buf_len, "line %d: %s", line_number, err != NULL ? err : "parse error");
            }
            vger_program_clear(program);
            return false;
        }

        program->steps[program->step_count] = step;
        program->step_count++;
    }

    VGER_ASSERT(program->step_count >= 0);
    VGER_ASSERT(program->step_count <= VGER_PROGRAM_MAX_LINES);
    return true;
}

bool vger_program_find_label(const vger_program_t *program, int label_number, int *out_index)
{
    VGER_ASSERT(program != NULL);
    VGER_ASSERT(out_index != NULL);

    for (int i = 0; i < program->step_count; i++) {
        const vger_program_step_t *step = &program->steps[i];
        if (step->kind == VGER_STEP_OPCODE_LABEL_ARG && step->opcode == VGER_KEY_LBL && step->int_arg == label_number) {
            *out_index = i;
            return true;
        }
    }
    return false;
}
