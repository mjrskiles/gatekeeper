#ifndef GK_OUTPUT_CV_OUTPUT_H
#define GK_OUTPUT_CV_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"
#include "utility/status.h"

/**
 * @file cv_output.h
 * @brief CV output handling with multiple output modes
 *
 * Provides gate, pulse, and toggle output modes for CV signal generation.
 */

#define PULSE_DURATION_MS 10

/**
 * CV output status flags
 *
 * Bit layout:
 *   [7:3] - reserved
 *   [2]   - CVOUT_LAST_IN: Last input state (for edge detection)
 *   [1]   - CVOUT_PULSE: Pulse currently active
 *   [0]   - CVOUT_STATE: Current output state (high/low)
 */
#define CVOUT_STATE     (1 << 0)    // Current output state
#define CVOUT_PULSE     (1 << 1)    // Pulse currently active
#define CVOUT_LAST_IN   (1 << 2)    // Last input state (for edge detect)
// Bits 3-7 reserved

/**
 * CV output state structure
 *
 * Memory layout (6 bytes, was 8):
 *   - pin:         1 byte
 *   - status:      1 byte (3 bools consolidated)
 *   - pulse_start: 4 bytes
 */
typedef struct CVOutput {
    uint8_t pin;            // Output pin number
    uint8_t status;         // Status flags (see CVOUT_* defines)
    uint32_t pulse_start;   // Pulse start timestamp
} CVOutput;

/**
 * Initialize CV output.
 *
 * @param cv_output  Pointer to CVOutput struct
 * @param pin        GPIO pin number for output
 */
void cv_output_init(CVOutput *cv_output, uint8_t pin);

/**
 * Reset CV output to initial state.
 *
 * Clears output and all flags. Call during mode transitions.
 *
 * @param cv_output  Pointer to CVOutput struct
 */
void cv_output_reset(CVOutput *cv_output);

/**
 * Set output high.
 *
 * @param cv_output  Pointer to CVOutput struct
 */
void cv_output_set(CVOutput *cv_output);

/**
 * Set output low.
 *
 * @param cv_output  Pointer to CVOutput struct
 */
void cv_output_clear(CVOutput *cv_output);

/**
 * Update output in gate mode.
 *
 * Output follows input directly.
 *
 * @param cv_output    Pointer to CVOutput struct
 * @param input_state  Current input state
 * @return             Current output state
 */
bool cv_output_update_gate(CVOutput *cv_output, bool input_state);

/**
 * Update output in pulse mode.
 *
 * Generates fixed-duration pulse on rising edge of input.
 *
 * @param cv_output    Pointer to CVOutput struct
 * @param input_state  Current input state
 * @return             Current output state
 */
bool cv_output_update_pulse(CVOutput *cv_output, bool input_state);

/**
 * Update output in toggle mode.
 *
 * Toggles output on rising edge of input.
 *
 * @param cv_output    Pointer to CVOutput struct
 * @param input_state  Current input state
 * @return             Current output state
 */
bool cv_output_update_toggle(CVOutput *cv_output, bool input_state);

#endif /* GK_OUTPUT_CV_OUTPUT_H */
