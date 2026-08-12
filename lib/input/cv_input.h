#ifndef GK_CV_INPUT_H
#define GK_CV_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file cv_input.h
 * @brief CV input processing with software hysteresis (per ADR-004)
 *
 * Implements a software Schmitt trigger for the analog CV input.
 * Converts 8-bit ADC readings (0-255) to digital state with configurable
 * thresholds and hysteresis for noise rejection.
 */

// ADC channel for CV input (PB3 = ADC3)
#define CV_ADC_CHANNEL 3

// Default thresholds (per ADR-004)
// High threshold: 2.5V = 128 (50% of 5V range)
// Low threshold:  1.5V = 77  (30% of 5V range)
// Hysteresis:     1.0V = 51  (20% of 5V range)
#define CV_DEFAULT_HIGH_THRESHOLD   128
#define CV_DEFAULT_LOW_THRESHOLD    77

/**
 * CV input state with hysteresis
 */
typedef struct {
    uint8_t high_threshold;     // ADC value to transition low->high
    uint8_t low_threshold;      // ADC value to transition high->low
    uint8_t last_adc_value;     // Most recent ADC reading (for diagnostics)
    bool current_state;         // Current digital state (after hysteresis)
} CVInput;

/**
 * Initialize CV input with default thresholds.
 *
 * @param cv    Pointer to CVInput struct
 */
void cv_input_init(CVInput *cv);

/**
 * Initialize CV input with custom thresholds.
 *
 * @param cv              Pointer to CVInput struct
 * @param high_threshold  ADC value to transition low->high
 * @param low_threshold   ADC value to transition high->low
 */
void cv_input_init_custom(CVInput *cv, uint8_t high_threshold, uint8_t low_threshold);

/**
 * Update CV input state from ADC reading.
 *
 * Applies hysteresis: state only changes when voltage crosses the
 * appropriate threshold (high_threshold to go HIGH, low_threshold to go LOW).
 *
 * @param cv          Pointer to CVInput struct
 * @param adc_value   8-bit ADC reading (0-255)
 * @return            Current digital state (true = HIGH)
 */
bool cv_input_update(CVInput *cv, uint8_t adc_value);

/**
 * Get current digital state.
 *
 * @param cv    Pointer to CVInput struct
 * @return      Current state (true = HIGH)
 */
bool cv_input_get_state(const CVInput *cv);

/**
 * Get most recent ADC reading.
 *
 * @param cv    Pointer to CVInput struct
 * @return      Last ADC value (0-255)
 */
uint8_t cv_input_get_adc_value(const CVInput *cv);

/**
 * Convert ADC value to approximate voltage (for display).
 *
 * @param adc_value   8-bit ADC reading
 * @return            Voltage in millivolts (0-5000)
 */
uint16_t cv_adc_to_millivolts(uint8_t adc_value);

#endif /* GK_CV_INPUT_H */
