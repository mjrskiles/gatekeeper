#ifndef GK_CORE_STATES_H
#define GK_CORE_STATES_H

/**
 * @file states.h
 * @brief State and page definitions for Gatekeeper FSM hierarchy
 *
 * Defines the states for the three-level FSM hierarchy:
 * - Top-level: PERFORM (normal operation) vs MENU (configuration)
 * - Mode: Which signal processing mode is active
 * - Menu: Which settings page is displayed
 *
 * See ADR-003 and AP-002 for design rationale.
 */

/**
 * Top-level states
 *
 * Controls whether the device is in normal operation (PERFORM)
 * or configuration mode (MENU).
 */
typedef enum {
    TOP_PERFORM = 0,    // Normal operation - process signals
    TOP_MENU,           // Configuration mode - adjust settings
    TOP_STATE_COUNT
} TopState;

/**
 * Mode states (signal processing modes)
 *
 * Each mode defines how button/CV input is transformed to output.
 * Mode persists across menu entry/exit and power cycles.
 */
typedef enum {
    MODE_GATE = 0,      // Output follows input (high while pressed)
    MODE_TRIGGER,       // Rising edge produces fixed-length pulse
    MODE_TOGGLE,        // Each press toggles output state
    MODE_DIVIDE,        // Output toggles every N inputs (clock divider)
    MODE_CYCLE,         // Cycles through pattern on each input
    MODE_COUNT
} ModeState;

/**
 * Menu pages (flat ring navigation)
 *
 * Settings pages are organized in a ring. A:tap advances to next page,
 * B:tap cycles the value on current page. Pages are grouped by mode
 * for context-aware menu entry.
 */
typedef enum {
    // Gate mode settings
    PAGE_GATE_CV = 0,           // CV input function for gate mode

    // Trigger mode settings
    PAGE_TRIGGER_BEHAVIOR,      // Edge detection (rise/fall/both)
    PAGE_TRIGGER_PULSE_LEN,     // Pulse duration

    // Toggle mode settings
    PAGE_TOGGLE_BEHAVIOR,       // Toggle behavior options

    // Divide mode settings
    PAGE_DIVIDE_DIVISOR,        // Division ratio (2, 3, 4, etc.)

    // Cycle mode settings
    PAGE_CYCLE_PATTERN,         // Cycle pattern selection

    // Global settings
    PAGE_CV_GLOBAL,             // Global CV input configuration
    PAGE_MENU_TIMEOUT,          // Menu auto-exit timeout

    PAGE_COUNT
} MenuPage;

/**
 * Mode to start page mapping
 *
 * When entering menu, jump to the first page relevant to current mode.
 * This provides context-aware menu navigation.
 */
static inline MenuPage mode_to_start_page(ModeState mode) {
    switch (mode) {
        case MODE_GATE:     return PAGE_GATE_CV;
        case MODE_TRIGGER:  return PAGE_TRIGGER_BEHAVIOR;
        case MODE_TOGGLE:   return PAGE_TOGGLE_BEHAVIOR;
        case MODE_DIVIDE:   return PAGE_DIVIDE_DIVISOR;
        case MODE_CYCLE:    return PAGE_CYCLE_PATTERN;
        default:            return PAGE_GATE_CV;
    }
}

#endif /* GK_CORE_STATES_H */
