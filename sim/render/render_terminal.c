#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/**
 * @file render_terminal.c
 * @brief ANSI terminal renderer for simulator
 *
 * Renders simulator state as a colorful terminal UI with:
 * - LED color display
 * - State/mode/page readout
 * - Input/output status
 * - Event log
 * - Optional legend
 */

typedef struct {
    struct termios orig_termios;
    bool terminal_raw;
    int last_event_count;
} TerminalCtx;

/**
 * Convert RGB to ANSI 256-color code (approximate).
 * Uses the 6x6x6 color cube (codes 16-231).
 */
static int rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b) {
    int ri = (r * 6) / 256;
    int gi = (g * 6) / 256;
    int bi = (b * 6) / 256;
    return 16 + (36 * ri) + (6 * gi) + bi;
}

static void enable_raw_mode(TerminalCtx *ctx) {
    if (ctx->terminal_raw) return;
    tcgetattr(STDIN_FILENO, &ctx->orig_termios);
    struct termios raw = ctx->orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    ctx->terminal_raw = true;
}

static void disable_raw_mode(TerminalCtx *ctx) {
    if (!ctx->terminal_raw) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx->orig_termios);
    ctx->terminal_raw = false;
}

static void draw_legend(void) {
    printf("\n\033[1m┌─ LED Color Legend ─────────────────────────────────┐\033[0m\n");
    printf("│  \033[1mMode LED:\033[0m                                          │\n");
    printf("│    \033[48;5;46m   \033[0m Green     GATE       \033[48;5;51m   \033[0m Cyan      TRIGGER   │\n");
    printf("│    \033[48;5;208m   \033[0m Orange    TOGGLE     \033[48;5;201m   \033[0m Magenta   DIVIDE    │\n");
    printf("│    \033[48;5;21m   \033[0m Blue      CYCLE                               │\n");
    printf("│  \033[1mActivity LED:\033[0m                                      │\n");
    printf("│    \033[48;5;231m   \033[0m White     Output active                      │\n");
    printf("│    \033[48;5;236m   \033[0m Off       Output inactive                    │\n");
    printf("└────────────────────────────────────────────────────┘\n");
}

static void terminal_init(Renderer *self) {
    TerminalCtx *ctx = (TerminalCtx*)self->ctx;
    enable_raw_mode(ctx);
    // Hide cursor and clear screen
    printf("\033[?25l\033[2J");
    fflush(stdout);
}

static void terminal_render(Renderer *self, const SimState *state) {
    TerminalCtx *ctx = (TerminalCtx*)self->ctx;
    (void)ctx;

    // Move cursor to top-left
    printf("\033[H");

    // Header
    printf("\033[1m=== Gatekeeper Simulator ===\033[0m              Time: %-10lu ms\033[K\n\n",
           (unsigned long)state->timestamp_ms);

    // LED display
    printf("  LEDs: ");
    const char* led_names[] = {"Mode", "Activity"};
    for (int i = 0; i < SIM_NUM_LEDS; i++) {
        int color = rgb_to_ansi256(state->leds[i].r, state->leds[i].g, state->leds[i].b);
        if (state->leds[i].r == 0 && state->leds[i].g == 0 && state->leds[i].b == 0) {
            printf("\033[48;5;236m   \033[0m ");  // Dark gray for off
        } else {
            printf("\033[48;5;%dm   \033[0m ", color);
        }
    }
    printf("  (%s / %s)\033[K\n\n", led_names[0], led_names[1]);

    // State/Mode/Page readout
    const char* page_str = state->in_menu ? sim_page_str(state->page) : "--";
    printf("  State: \033[1m%-10s\033[0m  Mode: \033[1m%-10s\033[0m  Page: \033[1m%-15s\033[0m\033[K\n\n",
           sim_top_state_str(state->top_state),
           sim_mode_str(state->mode),
           page_str);

    // Output state
    printf("  Output: %s\033[K\n\n",
           state->signal_out ? "\033[42;30m HIGH \033[0m" : "\033[100m LOW  \033[0m");

    // Input states
    printf("  Button A: %s    Button B: %s\033[K\n",
           state->button_a ? "\033[43;30m[HELD]\033[0m" : "[ -- ]",
           state->button_b ? "\033[43;30m[HELD]\033[0m" : "[ -- ]");

    // CV input display with voltage bar
    uint16_t cv_mv = (uint16_t)(((uint32_t)state->cv_voltage * 5000) / 255);
    int bar_width = (state->cv_voltage * 16) / 255;  // 16 char bar
    printf("  CV Input: %s  %u.%uV [",
           state->cv_in ? "\033[42;30m HIGH \033[0m" : "\033[100m LOW  \033[0m",
           cv_mv / 1000, (cv_mv % 1000) / 100);
    for (int i = 0; i < 16; i++) {
        if (i < bar_width) {
            printf("\033[46m \033[0m");  // Cyan bar
        } else {
            printf("\033[100m \033[0m");  // Dark gray
        }
    }
    printf("]\033[K\n\n");

    // Controls
    printf("\033[2m──────────────────────────────────────────────────────\033[0m\033[K\n");
    printf("  [A] Button A    [B] Button B    [C] Toggle CV\033[K\n");
    printf("  [+/-] CV Volts  [R] Reset       [L] Legend     [Q] Quit\033[K\n");
    printf("\033[2m──────────────────────────────────────────────────────\033[0m\033[K\n\n");

    // Legend (if visible)
    if (state->legend_visible) {
        draw_legend();
    } else {
        // Clear legend area
        for (int i = 0; i < 10; i++) {
            printf("\033[K\n");
        }
    }

    // Event log
    printf("\n\033[1mEvent Log:\033[0m\033[K\n");
    if (state->event_count == 0) {
        printf("  \033[2m(no events yet)\033[0m\033[K\n");
        for (int i = 1; i < SIM_MAX_EVENTS; i++) {
            printf("\033[K\n");
        }
    } else {
        int start = (state->event_count < SIM_MAX_EVENTS) ? 0 :
                    (state->event_head);
        int count = (state->event_count < SIM_MAX_EVENTS) ? state->event_count : SIM_MAX_EVENTS;

        for (int i = 0; i < SIM_MAX_EVENTS; i++) {
            if (i < count) {
                int idx = (start + i) % SIM_MAX_EVENTS;
                printf("  \033[36m%8lu ms\033[0m  %-50s\033[K\n",
                       (unsigned long)state->events[idx].time_ms,
                       state->events[idx].message);
            } else {
                printf("\033[K\n");
            }
        }
    }

    printf("\033[K\n");
    fflush(stdout);
}

static bool terminal_handle_input(Renderer *self, SimState *state, int key) {
    (void)self;

    switch (key) {
        case 'l': case 'L':
            sim_state_toggle_legend(state);
            return true;

        case 'q': case 'Q': case 27:  // ESC
            return false;

        default:
            return true;
    }
}

static void terminal_cleanup(Renderer *self) {
    TerminalCtx *ctx = (TerminalCtx*)self->ctx;
    disable_raw_mode(ctx);
    // Show cursor and clear screen
    printf("\033[?25h\033[H\033[J");
    printf("Simulator exited.\n");
    fflush(stdout);
}

Renderer* render_terminal_create(void) {
    Renderer *r = malloc(sizeof(Renderer));
    if (!r) return NULL;

    TerminalCtx *ctx = malloc(sizeof(TerminalCtx));
    if (!ctx) {
        free(r);
        return NULL;
    }

    ctx->terminal_raw = false;
    ctx->last_event_count = 0;

    r->init = terminal_init;
    r->render = terminal_render;
    r->handle_input = terminal_handle_input;
    r->cleanup = terminal_cleanup;
    r->ctx = ctx;

    return r;
}

void render_destroy(Renderer *renderer) {
    if (!renderer) return;
    if (renderer->ctx) free(renderer->ctx);
    free(renderer);
}
