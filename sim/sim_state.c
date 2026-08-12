#include "sim_state.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * @file sim_state.c
 * @brief Simulator state aggregation implementation
 */

// String tables for JSON/display output
static const char* event_type_strings[] = {
    [EVT_TYPE_STATE_CHANGE] = "state_change",
    [EVT_TYPE_MODE_CHANGE]  = "mode_change",
    [EVT_TYPE_PAGE_CHANGE]  = "page_change",
    [EVT_TYPE_INPUT]        = "input",
    [EVT_TYPE_OUTPUT]       = "output",
    [EVT_TYPE_INFO]         = "info"
};

static const char* top_state_strings[] = {
    [TOP_PERFORM] = "PERFORM",
    [TOP_MENU]    = "MENU"
};

static const char* mode_strings[] = {
    [MODE_GATE]    = "GATE",
    [MODE_TRIGGER] = "TRIGGER",
    [MODE_TOGGLE]  = "TOGGLE",
    [MODE_DIVIDE]  = "DIVIDE",
    [MODE_CYCLE]   = "CYCLE"
};

static const char* page_strings[] = {
    [PAGE_GATE_CV]           = "GATE_CV",
    [PAGE_TRIGGER_BEHAVIOR]  = "TRIGGER_BEHAVIOR",
    [PAGE_TRIGGER_PULSE_LEN] = "TRIGGER_PULSE_LEN",
    [PAGE_TOGGLE_BEHAVIOR]   = "TOGGLE_BEHAVIOR",
    [PAGE_DIVIDE_DIVISOR]    = "DIVIDE_DIVISOR",
    [PAGE_CYCLE_PATTERN]     = "CYCLE_PATTERN",
    [PAGE_CV_GLOBAL]         = "CV_GLOBAL",
    [PAGE_MENU_TIMEOUT]      = "MENU_TIMEOUT"
};

void sim_state_init(SimState *state) {
    if (!state) return;

    memset(state, 0, sizeof(SimState));
    state->version = SIM_STATE_VERSION;
    state->top_state = TOP_PERFORM;
    state->mode = MODE_GATE;
    state->page = PAGE_GATE_CV;
    state->in_menu = false;
    state->legend_visible = false;
    state->dirty = true;
}

void sim_state_set_fsm(SimState *state, TopState top, ModeState mode, MenuPage page, bool in_menu) {
    if (!state) return;

    if (state->top_state != top || state->mode != mode ||
        state->page != page || state->in_menu != in_menu) {
        state->top_state = top;
        state->mode = mode;
        state->page = page;
        state->in_menu = in_menu;
        state->dirty = true;
    }
}

void sim_state_set_inputs(SimState *state, bool btn_a, bool btn_b, bool cv_digital, uint8_t cv_voltage) {
    if (!state) return;

    if (state->button_a != btn_a || state->button_b != btn_b ||
        state->cv_in != cv_digital || state->cv_voltage != cv_voltage) {
        state->button_a = btn_a;
        state->button_b = btn_b;
        state->cv_in = cv_digital;
        state->cv_voltage = cv_voltage;
        state->dirty = true;
    }
}

void sim_state_set_output(SimState *state, bool signal) {
    if (!state) return;

    if (state->signal_out != signal) {
        state->signal_out = signal;
        state->dirty = true;
    }
}

void sim_state_set_time(SimState *state, uint32_t time_ms) {
    if (!state) return;

    if (state->timestamp_ms != time_ms) {
        state->timestamp_ms = time_ms;
        state->dirty = true;
    }
}

void sim_state_set_led(SimState *state, int index, uint8_t r, uint8_t g, uint8_t b) {
    if (!state) return;
    if (index < 0 || index >= SIM_NUM_LEDS) return;

    if (state->leds[index].r != r || state->leds[index].g != g || state->leds[index].b != b) {
        state->leds[index].r = r;
        state->leds[index].g = g;
        state->leds[index].b = b;
        state->dirty = true;
    }
}

void sim_state_add_event(SimState *state, SimEventType type, uint32_t time_ms, const char *fmt, ...) {
    if (!state) return;

    SimStateEvent *evt = &state->events[state->event_head];
    evt->time_ms = time_ms;
    evt->type = type;

    va_list args;
    va_start(args, fmt);
    vsnprintf(evt->message, sizeof(evt->message), fmt, args);
    va_end(args);

    state->event_head = (state->event_head + 1) % SIM_MAX_EVENTS;
    if (state->event_count < SIM_MAX_EVENTS) {
        state->event_count++;
    }
    state->dirty = true;
}

void sim_state_toggle_legend(SimState *state) {
    if (!state) return;
    state->legend_visible = !state->legend_visible;
    state->dirty = true;
}

bool sim_state_is_dirty(const SimState *state) {
    if (!state) return false;
    return state->dirty;
}

void sim_state_clear_dirty(SimState *state) {
    if (!state) return;
    state->dirty = false;
}

void sim_state_mark_dirty(SimState *state) {
    if (!state) return;
    state->dirty = true;
}

const char* sim_event_type_str(SimEventType type) {
    if (type > EVT_TYPE_INFO) return "unknown";
    return event_type_strings[type];
}

const char* sim_top_state_str(TopState state) {
    if (state > TOP_MENU) return "???";
    return top_state_strings[state];
}

const char* sim_mode_str(ModeState mode) {
    if (mode >= MODE_COUNT) return "???";
    return mode_strings[mode];
}

const char* sim_page_str(MenuPage page) {
    if (page >= PAGE_COUNT) return "???";
    return page_strings[page];
}
