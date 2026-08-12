#ifndef GK_CORE_COORDINATOR_H
#define GK_CORE_COORDINATOR_H

#include "fsm/fsm.h"
#include "events/events.h"
#include "core/states.h"
#include "modes/mode_handlers.h"
#include "input/cv_input.h"
#include "app_init.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file coordinator.h
 * @brief Gatekeeper application coordinator
 *
 * Coordinates the three-level FSM hierarchy:
 * - Top FSM: PERFORM <-> MENU transitions
 * - Mode FSM: Current signal processing mode
 * - Menu FSM: Settings page navigation
 *
 * Also manages the event processor and routes events to the appropriate FSM.
 *
 * See AP-002 for implementation details.
 */

// Menu timeout (auto-exit after inactivity)
#define MENU_TIMEOUT_MS     60000   // 60 seconds

/**
 * Gatekeeper application coordinator
 *
 * Holds all three FSM instances and shared context.
 */
typedef struct {
    // FSM instances
    FSM top_fsm;            // PERFORM <-> MENU
    FSM mode_fsm;           // Mode selection (Gate, Trigger, etc.)
    FSM menu_fsm;           // Menu page navigation

    // Event processing
    EventProcessor events;  // Input event detection

    // CV input processing (per ADR-004)
    CVInput cv_input;       // Analog CV with hysteresis

    // Context
    uint8_t menu_entry_mode;    // Mode when menu was entered (for context restore)
    uint32_t menu_enter_time;   // Timestamp for timeout tracking
    uint32_t last_activity;     // Last user interaction time

    // Settings reference (for save on menu exit)
    AppSettings *settings;

    // Mode handler context (union - only one mode active at a time)
    ModeContext mode_ctx;

    // Output state (set by mode handlers, read by main loop)
    bool output_state;          // Current output pin state
} Coordinator;

/**
 * Initialize the Gatekeeper coordinator.
 *
 * Sets up all three FSMs with their state and transition tables.
 * Does not start the FSMs - call coordinator_start() after init.
 *
 * @param coord     Pointer to Coordinator struct
 * @param settings  Pointer to app settings (for persistence)
 */
void coordinator_init(Coordinator *coord, AppSettings *settings);

/**
 * Start the coordinator.
 *
 * Activates all FSMs and enters initial states.
 * Call after coordinator_init() and setting initial mode from settings.
 *
 * @param coord Pointer to Coordinator struct
 */
void coordinator_start(Coordinator *coord);

/**
 * Update the coordinator.
 *
 * Call once per main loop iteration. Reads inputs, processes events,
 * updates FSMs, and runs mode handlers.
 *
 * @param coord Pointer to Coordinator struct
 */
void coordinator_update(Coordinator *coord);

/**
 * Get the top-level FSM state.
 *
 * @param coord Pointer to Coordinator struct
 * @return      Current TopState (TOP_PERFORM or TOP_MENU)
 */
TopState coordinator_get_top_state(const Coordinator *coord);

/**
 * Get the current operating mode.
 *
 * @param coord Pointer to Coordinator struct
 * @return      Current ModeState
 */
ModeState coordinator_get_mode(const Coordinator *coord);

/**
 * Set the operating mode directly.
 *
 * Used for restoring mode from settings on startup.
 *
 * @param coord Pointer to Coordinator struct
 * @param mode  Mode to set
 */
void coordinator_set_mode(Coordinator *coord, ModeState mode);

/**
 * Check if currently in menu mode.
 *
 * @param coord Pointer to Coordinator struct
 * @return      true if in menu, false if in perform mode
 */
bool coordinator_in_menu(const Coordinator *coord);

/**
 * Get the current menu page.
 *
 * Only meaningful when in menu mode.
 *
 * @param coord Pointer to Coordinator struct
 * @return      Current MenuPage
 */
MenuPage coordinator_get_page(const Coordinator *coord);

/**
 * Get the current output state.
 *
 * Returns the output state as determined by the current mode handler.
 *
 * @param coord Pointer to Coordinator struct
 * @return      true if output should be HIGH
 */
bool coordinator_get_output(const Coordinator *coord);

/**
 * Get LED feedback for current state.
 *
 * Fills the feedback struct with mode color and activity indicator.
 *
 * @param coord     Pointer to Coordinator struct
 * @param feedback  Pointer to LEDFeedback struct to fill
 */
void coordinator_get_led_feedback(const Coordinator *coord, LEDFeedback *feedback);

/**
 * Get the current CV input state (after hysteresis).
 *
 * Returns the digital state of the CV input after applying
 * software hysteresis (Schmitt trigger behavior).
 *
 * @param coord Pointer to Coordinator struct
 * @return      true if CV is HIGH (above threshold)
 */
bool coordinator_get_cv_state(const Coordinator *coord);

#endif /* GK_CORE_COORDINATOR_H */
