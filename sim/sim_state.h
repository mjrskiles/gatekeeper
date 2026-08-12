#ifndef SIM_STATE_H
#define SIM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/states.h"

/**
 * @file sim_state.h
 * @brief Simulator state aggregation
 *
 * Central state structure that collects all simulator state for rendering.
 * Renderers consume this struct to produce output (terminal UI, JSON, etc).
 */

#define SIM_MAX_EVENTS 16
#define SIM_NUM_LEDS 2
#define SIM_STATE_VERSION 1

/**
 * Event types for state change logging
 */
typedef enum {
    EVT_TYPE_STATE_CHANGE,
    EVT_TYPE_MODE_CHANGE,
    EVT_TYPE_PAGE_CHANGE,
    EVT_TYPE_INPUT,
    EVT_TYPE_OUTPUT,
    EVT_TYPE_INFO
} SimEventType;

/**
 * Logged event
 */
typedef struct {
    uint32_t time_ms;
    SimEventType type;
    char message[128];
} SimStateEvent;

/**
 * LED state
 */
typedef struct {
    uint8_t r, g, b;
} SimLED;

/**
 * Complete simulator state snapshot
 */
typedef struct {
    // Schema version
    int version;

    // Timestamp
    uint32_t timestamp_ms;

    // FSM state
    TopState top_state;
    ModeState mode;
    MenuPage page;
    bool in_menu;

    // Inputs
    bool button_a;
    bool button_b;
    bool cv_in;             // Digital CV state (after hysteresis)
    uint8_t cv_voltage;     // Raw CV voltage (0-255 ADC value)

    // Outputs
    bool signal_out;

    // LEDs
    SimLED leds[SIM_NUM_LEDS];

    // Recent events (circular buffer)
    SimStateEvent events[SIM_MAX_EVENTS];
    int event_head;
    int event_count;

    // Display hints (for terminal renderer)
    bool legend_visible;

    // Dirty tracking
    bool dirty;
} SimState;

/**
 * Initialize state struct with defaults.
 */
void sim_state_init(SimState *state);

/**
 * Update FSM state.
 */
void sim_state_set_fsm(SimState *state, TopState top, ModeState mode, MenuPage page, bool in_menu);

/**
 * Update input states.
 */
void sim_state_set_inputs(SimState *state, bool btn_a, bool btn_b, bool cv_digital, uint8_t cv_voltage);

/**
 * Update output state.
 */
void sim_state_set_output(SimState *state, bool signal);

/**
 * Update timestamp.
 */
void sim_state_set_time(SimState *state, uint32_t time_ms);

/**
 * Update LED state.
 */
void sim_state_set_led(SimState *state, int index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Add an event to the log.
 */
void sim_state_add_event(SimState *state, SimEventType type, uint32_t time_ms, const char *fmt, ...);

/**
 * Toggle legend visibility.
 */
void sim_state_toggle_legend(SimState *state);

/**
 * Check if state has changed since last render.
 */
bool sim_state_is_dirty(const SimState *state);

/**
 * Clear dirty flag after render.
 */
void sim_state_clear_dirty(SimState *state);

/**
 * Mark state as dirty (force redraw).
 */
void sim_state_mark_dirty(SimState *state);

/**
 * Get event type as string for JSON output.
 */
const char* sim_event_type_str(SimEventType type);

/**
 * Get top state as string.
 */
const char* sim_top_state_str(TopState state);

/**
 * Get mode as string.
 */
const char* sim_mode_str(ModeState mode);

/**
 * Get page as string (or NULL if not in menu).
 */
const char* sim_page_str(MenuPage page);

#endif /* SIM_STATE_H */
