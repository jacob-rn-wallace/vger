/**
 * @file main.c
 * @brief Desktop SDL2 test harness for the vger FOCAL core (milestone 1).
 *
 * Purpose: prove the core's architecture (state/query API, swappable
 * mode-boundary policy, out-of-band reset, RUN/PRGM key dispatch) works
 * end to end, before any Pico 2/display/keypad hardware bring-up. Renders
 * the display line as seven-segment digits in a 400x240 logical canvas
 * (matching the real Sharp Memory LCD target's resolution) and dumps full
 * register/flag/alpha state to the terminal after every keystroke, which
 * exercises vger_state.h's query API far more thoroughly than a hand-built
 * on-screen text font would.
 */

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "vger_assert.h"
#include "vger_interp.h"
#include "vger_keymap.h"
#include "vger_mode_policy.h"
#include "vger_program.h"
#include "vger_sevenseg.h"
#include "vger_state.h"

#define VGER_LOGICAL_WIDTH 400
#define VGER_LOGICAL_HEIGHT 240
#define VGER_WINDOW_SCALE 2
#define VGER_MAX_EVENTS_PER_FRAME 256
#define VGER_DISPLAY_MAX_CHARS 12
#define VGER_PROGRAM_FILE_MAX_BYTES 32768

/** @brief Read a text-format FOCAL program from path and load it into
 *  state, as an alternative to keystroke (PRGM-mode) programming.
 *  @return true on success; prints a diagnostic and returns false on any
 *          read or parse failure (state is left untouched on failure). */
static bool vger_load_program_file(const char *path, vger_state_t *state)
{
    VGER_ASSERT(path != NULL);
    VGER_ASSERT(state != NULL);

    static char file_buffer[VGER_PROGRAM_FILE_MAX_BYTES];
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        (void)fprintf(stderr, "vger: could not open program file '%s'\n", path);
        return false;
    }
    size_t read_len = fread(file_buffer, 1, sizeof(file_buffer) - 1U, f);
    (void)fclose(f);
    file_buffer[read_len] = '\0';

    vger_program_t program;
    char err[128];
    if (!vger_program_parse_text(file_buffer, &program, err, sizeof(err))) {
        (void)fprintf(stderr, "vger: failed to parse '%s': %s\n", path, err);
        return false;
    }

    vger_state_load_program(state, &program);
    printf("[LOAD] parsed %d steps from '%s'\n", vger_get_program_step_count(state), path);
    return true;
}

static void vger_print_keymap_legend(void)
{
    printf("vger desktop test harness -- keymap\n");
    printf("  0-9, .            digit entry\n");
    printf("  Enter             ENTER^\n");
    printf("  Backspace         backspace / cancel last alpha char\n");
    printf("  KP + - * /        arithmetic\n");
    printf("  N                 CHS\n");
    printf("  Tab               X<>Y\n");
    printf("  Delete            CLX\n");
    printf("  L                 LASTX\n");
    printf("  I                 IND (indirect prefix; press right after STO/RCL/GTO/etc.,\n");
    printf("                    before its digits, except LBL which has no indirect form)\n");
    printf("  F1 F2 F3 F4       STO  RCL  GTO  LBL   (each followed by 2 digits)\n");
    printf("  F5 F6 F7          SF   CF   FS?C       (each followed by 2 digits)\n");
    printf("  F8 F9             ASTO ARCL             (each followed by 2 digits)\n");
    printf("  F10 , ;           FIX  SCI  ENG        (each followed by 1 digit)\n");
    printf("  X                 XEQ (followed by 2 digits; calls a subroutine)\n");
    printf("  R                 RTN (returns from a subroutine)\n");
    printf("  Space             R/S (run program from current line)\n");
    printf("  P                 PRGM (toggle program-entry mode)\n");
    printf("  U                 USER (toggle, annunciator only in milestone 1)\n");
    printf("  Home              ON (power toggle)\n");
    printf("  Escape            ALPHA (toggle; while on, letters/digits/space/period type text)\n");
    printf("  Insert            MENU (5th key; gated by the mode-boundary policy, stubbed)\n");
    printf("  F12               RESET (out-of-band, bypasses normal key dispatch)\n");
    printf("  window close      quit\n\n");
}

static void vger_print_state_dump(const vger_state_t *state)
{
    VGER_ASSERT(state != NULL);

    char alpha[VGER_ALPHA_BUFFER_LEN + 1];
    (void)vger_get_alpha_buffer(state, alpha, sizeof(alpha));
    char display[VGER_DISPLAY_STRING_LEN];
    (void)vger_get_display_string(state, display, sizeof(display));
    vger_annunciator_state_t ann = vger_get_annunciators(state);

    printf("---- state ----\n");
    printf("display: \"%s\"\n", display);
    printf("X=%g Y=%g Z=%g T=%g LASTX=%g\n", vger_get_x(state), vger_get_y(state), vger_get_z(state), vger_get_t(state),
           vger_get_lastx(state));
    printf("alpha=\"%s\" (%zu chars)\n", alpha, strlen(alpha));
    printf("mode=%s alpha_mode=%d user_mode=%d running=%d power=%d\n",
           vger_get_calc_mode(state) == VGER_CALC_MODE_PRGM ? "PRGM" : "RUN", ann.alpha_mode, ann.user_mode,
           ann.program_running, vger_get_power_on(state));
    printf("current_line=%d program_steps=%d call_stack_depth=%d\n", vger_get_current_line(state),
           vger_get_program_step_count(state), vger_get_call_stack_depth(state));

    printf("flags set:");
    bool any_flag = false;
    for (int i = 0; i <= VGER_MAX_USER_FLAG; i++) {
        if (vger_get_flag(state, i)) {
            printf(" %02d", i);
            any_flag = true;
        }
    }
    printf(any_flag ? "\n" : " (none)\n");
    printf("----------------\n");
}

/** @brief Handle one SDL_KEYDOWN event: classify it and route it to the
 *  core, the (stubbed) menu policy check, or the out-of-band reset. */
static void vger_handle_keydown(vger_state_t *state, SDL_Scancode scancode)
{
    VGER_ASSERT(state != NULL);

    bool alpha_active = vger_get_annunciators(state).alpha_mode;
    vger_key_event_t event;
    vger_host_action_t action = vger_keymap_classify(scancode, alpha_active, &event);

    switch (action) {
        case VGER_HOST_ACTION_CORE_KEY:
            vger_interp_handle_key(state, event);
            vger_print_state_dump(state);
            break;
        case VGER_HOST_ACTION_MENU:
            if (vger_mode_policy_may_enter_menu(VGER_MODE_POLICY_IDLE_ONLY, state)) {
                printf("[MENU] idle-only policy allowed entry (menu layer not implemented in milestone 1)\n");
            } else {
                printf("[MENU] idle-only policy refused entry (calculator is mid-operation)\n");
            }
            break;
        case VGER_HOST_ACTION_RESET:
            vger_state_reset(state);
            printf("[RESET] calculator state reset (out-of-band, bypassed normal key dispatch)\n");
            vger_print_state_dump(state);
            break;
        case VGER_HOST_ACTION_NONE:
        default:
            break;
    }
}

/** @brief Render one frame: background, seven-segment display line, and
 *  four annunciator squares (PRGM, ALPHA, USER, RUNNING, left to right). */
static void vger_render_frame(SDL_Renderer *renderer, const vger_state_t *state)
{
    VGER_ASSERT(renderer != NULL);
    VGER_ASSERT(state != NULL);

    (void)SDL_SetRenderDrawColor(renderer, 0xE8, 0xEC, 0xE4, 0xFF);
    (void)SDL_RenderClear(renderer);
    (void)SDL_SetRenderDrawColor(renderer, 0x10, 0x14, 0x10, 0xFF);

    char display[VGER_DISPLAY_STRING_LEN];
    (void)vger_get_display_string(state, display, sizeof(display));
    vger_sevenseg_draw_string(renderer, display, 12, 20, 26, 60, VGER_DISPLAY_MAX_CHARS);

    vger_annunciator_state_t ann = vger_get_annunciators(state);
    bool lit[4] = {ann.prgm_mode, ann.alpha_mode, ann.user_mode, ann.program_running};
    for (int i = 0; i < 4; i++) {
        SDL_Rect box = {12 + (i * 40), 110, 24, 24};
        if (lit[i]) {
            (void)SDL_RenderFillRect(renderer, &box);
        } else {
            (void)SDL_RenderDrawRect(renderer, &box);
        }
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    /* Force line-buffering even when stdout isn't a TTY (piped to a log
     * file, captured by a test harness), so state dumps show up promptly
     * instead of sitting in a full-buffer until process exit. */
    (void)setvbuf(stdout, NULL, _IOLBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        (void)fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("vger (milestone 1)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           VGER_LOGICAL_WIDTH * VGER_WINDOW_SCALE, VGER_LOGICAL_HEIGHT * VGER_WINDOW_SCALE,
                                           SDL_WINDOW_SHOWN);
    if (window == NULL) {
        (void)fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        (void)fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    (void)SDL_RenderSetLogicalSize(renderer, VGER_LOGICAL_WIDTH, VGER_LOGICAL_HEIGHT);

    vger_state_t *state = vger_state_get();
    vger_state_reset(state);
    if (argc > 1) {
        (void)vger_load_program_file(argv[1], state);
    }
    vger_print_keymap_legend();
    vger_print_state_dump(state);

    bool running = true;
    while (running) { /* justified unbounded loop: an interactive GUI event
                        * loop has no natural termination bound besides the
                        * user quitting; see DEVIATIONS.md. */
        SDL_Event ev;
        for (int i = 0; i < VGER_MAX_EVENTS_PER_FRAME && SDL_PollEvent(&ev) != 0; i++) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
                vger_handle_keydown(state, ev.key.keysym.scancode);
            }
        }
        vger_render_frame(renderer, state);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
