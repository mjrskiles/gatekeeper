#ifndef GK_INPUT_BUTTON_H
#define GK_INPUT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"
#include "utility/status.h"

/**
 * @file button.h
 * @brief Button input handling with debounce and edge detection
 *
 * Provides debounced button state, rising/falling edge detection,
 * and config action detection (5-tap + hold gesture).
 */

// Timing constants
#define TAP_TIMEOUT_MS      500     // Max time between taps for config action
#define TAPS_TO_CHANGE      5       // Number of taps required
#define HOLD_TIME_MS        1000    // Hold duration after final tap
#define EDGE_DEBOUNCE_MS    5       // Debounce time for edge detection

/**
 * Button status flags
 *
 * Bit layout:
 *   [7]     - reserved
 *   [6]     - BTN_COUNTING: Counting hold time for config action
 *   [5]     - BTN_CONFIG: Config action detected (consumable)
 *   [4]     - BTN_FALL: Falling edge detected this cycle
 *   [3]     - BTN_RISE: Rising edge detected this cycle
 *   [2]     - BTN_LAST: Previous debounced state
 *   [1]     - BTN_PRESSED: Current debounced pressed state
 *   [0]     - BTN_RAW: Raw pin state (before debounce)
 */
#define BTN_RAW         (1 << 0)    // Raw pin state
#define BTN_PRESSED     (1 << 1)    // Debounced pressed state
#define BTN_LAST        (1 << 2)    // Previous pressed state
#define BTN_RISE        (1 << 3)    // Rising edge this cycle
#define BTN_FALL        (1 << 4)    // Falling edge this cycle
#define BTN_CONFIG      (1 << 5)    // Config action triggered
#define BTN_COUNTING    (1 << 6)    // Counting hold time
// Bit 7 reserved

/**
 * Button state structure
 *
 * Memory layout (15 bytes, was 21):
 *   - pin:            1 byte
 *   - status:         1 byte (7 bools consolidated)
 *   - tap_count:      1 byte
 *   - last_rise_time: 4 bytes
 *   - last_fall_time: 4 bytes
 *   - last_tap_time:  4 bytes
 */
typedef struct Button {
    uint8_t pin;                // Pin number
    uint8_t status;             // Status flags (see BTN_* defines)
    uint8_t tap_count;          // Number of taps (for config action)
    uint32_t last_rise_time;    // Timestamp of last rising edge
    uint32_t last_fall_time;    // Timestamp of last falling edge
    uint32_t last_tap_time;     // Timestamp of last tap (for config action)
} Button;

/**
 * Initialize button state.
 *
 * @param button  Pointer to Button struct
 * @param pin     GPIO pin number
 * @return        true if initialized successfully
 */
bool button_init(Button *button, uint8_t pin);

/**
 * Reset button to initial state.
 *
 * Clears all flags and timing state. Call after handling
 * special events or during mode transitions.
 *
 * @param button  Pointer to Button struct
 */
void button_reset(Button *button);

/**
 * Update button state.
 *
 * Call once per main loop iteration. Updates debounced state,
 * detects edges, and checks for config action.
 *
 * After calling, check:
 *   - STATUS_ANY(button->status, BTN_PRESSED) for pressed state
 *   - STATUS_ANY(button->status, BTN_RISE) for rising edge
 *   - STATUS_ANY(button->status, BTN_FALL) for falling edge
 *   - STATUS_ANY(button->status, BTN_CONFIG) for config action
 *
 * @param button  Pointer to Button struct
 */
void button_update(Button *button);

/**
 * LEGACY: Consume the config action flag.
 *
 * NOTE: This function is part of the legacy 5-tap config gesture system.
 * Mode changes now use compound gestures (EVT_MODE_NEXT) via the event
 * processor in src/events/events.c. Retained for potential alternate builds.
 *
 * @param button  Pointer to Button struct
 */
void button_consume_config_action(Button *button);

/**
 * LEGACY: Check for config action gesture.
 *
 * NOTE: This function is NOT currently used by the coordinator.
 * Mode changes now use compound gestures (hold B, then hold A) detected
 * by the event processor. See EVT_MODE_NEXT in include/events/events.h.
 *
 * Original behavior: Detects 5-tap + 1-second-hold gesture.
 * Retained for potential alternate builds or future use.
 *
 * @param button  Pointer to Button struct
 * @return        true if config action detected this cycle
 */
bool button_detect_config_action(Button *button);

/**
 * Check for rising edge with debounce.
 *
 * @param button  Pointer to Button struct
 * @return        true if rising edge detected
 */
bool button_has_rising_edge(Button *button);

/**
 * Check for falling edge with debounce.
 *
 * @param button  Pointer to Button struct
 * @return        true if falling edge detected
 */
bool button_has_falling_edge(Button *button);

#endif /* GK_INPUT_BUTTON_H */
