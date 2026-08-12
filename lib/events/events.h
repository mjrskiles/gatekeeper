#ifndef GK_EVENTS_H
#define GK_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "utility/status.h"

/**
 * @file events.h
 * @brief Event types and processor for Gatekeeper
 *
 * Transforms raw button/CV inputs into semantic events with proper
 * timing for press vs release, tap vs hold, and compound gestures.
 *
 * See FDP-004 for design rationale.
 */

/**
 * Event types
 *
 * Events are categorized by timing:
 * - PRESS events: Fire immediately on button down (performance-critical)
 * - TAP/RELEASE events: Fire on button up (configuration actions)
 * - HOLD events: Fire after threshold while still pressed
 * - Compound events: Detected from button combinations
 */
typedef enum {
    EVT_NONE = 0,

    // === Performance events (on press, fast response) ===
    EVT_A_PRESS,            // Button A pressed
    EVT_B_PRESS,            // Button B pressed
    EVT_CV_RISE,            // CV input rising edge
    EVT_CV_FALL,            // CV input falling edge

    // === Configuration events (on release, deliberate) ===
    EVT_A_TAP,              // Button A short press + release
    EVT_A_RELEASE,          // Button A released (any duration)
    EVT_B_TAP,              // Button B short press + release
    EVT_B_RELEASE,          // Button B released (any duration)

    // === Hold events (threshold reached while held) ===
    EVT_A_HOLD,             // Button A held past threshold
    EVT_B_HOLD,             // Button B held past threshold

    // === Compound/gesture events ===
    EVT_MENU_TOGGLE,        // A:hold + B:hold (A first, then B reaches threshold)
    EVT_MODE_NEXT,          // A:hold (solo) release (no B pressed during hold)

    // === Timing events ===
    EVT_TIMEOUT,            // Generic timeout (context-dependent)

    EVT_COUNT               // Number of events (for array sizing)
} Event;

/**
 * Event processor status flags (per ADR-002)
 *
 * Bit layout:
 *   [7] - EP_CV_LAST: Previous CV state
 *   [6] - EP_CV_STATE: Current CV state
 *   [5] - EP_B_HOLD: Button B hold threshold reached
 *   [4] - EP_B_LAST: Previous button B state
 *   [3] - EP_B_PRESSED: Button B currently pressed
 *   [2] - EP_A_HOLD: Button A hold threshold reached
 *   [1] - EP_A_LAST: Previous button A state
 *   [0] - EP_A_PRESSED: Button A currently pressed
 *
 * Extended flags (bits 8-15, uses upper byte of uint16_t if needed):
 *   For now we steal bit 7 for compound fired since CV_LAST is rarely needed
 *   Actually, let's use a separate byte in the struct instead.
 */
#define EP_A_PRESSED    (1 << 0)
#define EP_A_LAST       (1 << 1)
#define EP_A_HOLD       (1 << 2)
#define EP_B_PRESSED    (1 << 3)
#define EP_B_LAST       (1 << 4)
#define EP_B_HOLD       (1 << 5)
#define EP_CV_STATE     (1 << 6)
#define EP_CV_LAST      (1 << 7)

// Extended status flags (separate byte)
#define EP_COMPOUND_FIRED       (1 << 0)  // Compound gesture already fired this press
#define EP_B_TOUCHED_DURING_A   (1 << 1)  // B was pressed while A was held

/**
 * Timing thresholds (milliseconds)
 */
#define EP_HOLD_THRESHOLD_MS    500     // Time to trigger hold event
#define EP_TAP_THRESHOLD_MS     300     // Max duration for tap (vs hold)

/**
 * Event processor state
 *
 * Tracks button and CV input states for edge detection and
 * gesture recognition. Uses status word per ADR-002.
 */
typedef struct {
    uint8_t status;             // Input states (see EP_* flags)
    uint8_t ext_status;         // Extended status (see EP_COMPOUND_* flags)
    uint32_t a_press_time;      // Button A press timestamp
    uint32_t b_press_time;      // Button B press timestamp
} EventProcessor;

/**
 * Input state for event processor
 *
 * Passed to event_processor_update() each cycle.
 * Abstracts the source of inputs (HAL, mock, etc.)
 */
typedef struct {
    bool button_a;              // Button A state (true = pressed)
    bool button_b;              // Button B state (true = pressed)
    bool cv_in;                 // CV input state (true = high)
    uint32_t current_time;      // Current timestamp (ms)
} EventInput;

/**
 * Initialize event processor.
 *
 * Clears all state flags and timestamps.
 *
 * @param ep    Pointer to EventProcessor struct
 */
void event_processor_init(EventProcessor *ep);

/**
 * Reset event processor state.
 *
 * Clears all flags but preserves configuration.
 *
 * @param ep    Pointer to EventProcessor struct
 */
void event_processor_reset(EventProcessor *ep);

/**
 * Update event processor and return next event.
 *
 * Call once per main loop iteration with current input states.
 * Detects edges, hold thresholds, and compound gestures.
 *
 * Only one event is returned per call. Priority order:
 * 1. Compound gestures (menu enter, mode change)
 * 2. Hold events
 * 3. Release/tap events
 * 4. Press events
 * 5. CV events
 *
 * @param ep    Pointer to EventProcessor struct
 * @param input Current input states
 * @return      Detected event, or EVT_NONE if no event
 */
Event event_processor_update(EventProcessor *ep, const EventInput *input);

/**
 * Check if button A is currently pressed.
 *
 * @param ep    Pointer to EventProcessor struct
 * @return      true if button A is pressed
 */
bool event_processor_a_pressed(const EventProcessor *ep);

/**
 * Check if button B is currently pressed.
 *
 * @param ep    Pointer to EventProcessor struct
 * @return      true if button B is pressed
 */
bool event_processor_b_pressed(const EventProcessor *ep);

/**
 * Check if button A hold threshold has been reached.
 *
 * @param ep    Pointer to EventProcessor struct
 * @return      true if A is held past threshold
 */
bool event_processor_a_holding(const EventProcessor *ep);

/**
 * Check if button B hold threshold has been reached.
 *
 * @param ep    Pointer to EventProcessor struct
 * @return      true if B is held past threshold
 */
bool event_processor_b_holding(const EventProcessor *ep);

#endif /* GK_EVENTS_H */
