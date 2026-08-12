#ifndef GK_SIM_HAL_H
#define GK_SIM_HAL_H

#include "hardware/hal_interface.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file sim_hal.h
 * @brief x86 simulator HAL interface
 *
 * Pure hardware abstraction layer for x86 simulator.
 * Display is handled separately by renderers.
 */

// Number of simulated LEDs
#define SIM_NUM_LEDS 2

// =============================================================================
// Fault Injection API (FDP-016)
// =============================================================================

/**
 * ADC fault injection modes.
 * See FDP-016 for design rationale.
 */
typedef enum {
    SIM_ADC_NORMAL,      // Return actual CV value
    SIM_ADC_TIMEOUT,     // Always return 128 (matches hardware timeout behavior)
    SIM_ADC_STUCK_LOW,   // Always return 0
    SIM_ADC_STUCK_HIGH,  // Always return 255
    SIM_ADC_NOISY,       // Add random noise to readings
} SimAdcMode;

/**
 * EEPROM fault injection modes.
 * See FDP-016 Phase 3 for design rationale.
 */
typedef enum {
    SIM_EEPROM_NORMAL,      // Normal operation
    SIM_EEPROM_WRITE_FAIL,  // Writes silently fail (reads work)
    SIM_EEPROM_READ_FF,     // All reads return 0xFF (erased state)
    SIM_EEPROM_CORRUPT,     // Random bit flips on read
} SimEepromMode;

/**
 * Set ADC fault injection mode.
 * Faults persist until explicitly cleared with SIM_ADC_NORMAL.
 */
void sim_adc_set_mode(SimAdcMode mode);

/**
 * Set noise amplitude for SIM_ADC_NOISY mode.
 * Noise is +/- amplitude from the actual value.
 */
void sim_adc_set_noise_amplitude(uint8_t amplitude);

/**
 * Get current ADC fault mode.
 */
SimAdcMode sim_adc_get_mode(void);

/**
 * Set EEPROM fault injection mode.
 * Faults persist until explicitly cleared with SIM_EEPROM_NORMAL.
 */
void sim_eeprom_set_mode(SimEepromMode mode);

/**
 * Get current EEPROM fault mode.
 */
SimEepromMode sim_eeprom_get_mode(void);

/**
 * Get the simulator HAL interface.
 * Assign to p_hal before running app code.
 */
HalInterface* sim_get_hal(void);

/**
 * Input state setters (for input sources).
 * These set the simulated hardware input state.
 */
void sim_set_button_a(bool pressed);
void sim_set_button_b(bool pressed);

/**
 * Set CV input voltage level (0-255, maps to 0-5V).
 * The digital state is derived via hysteresis in cv_input module.
 */
void sim_set_cv_voltage(uint8_t adc_value);

/**
 * Adjust CV voltage by delta (-255 to +255).
 * Clamps to valid range.
 */
void sim_adjust_cv_voltage(int16_t delta);

/**
 * Input state getters.
 */
bool sim_get_button_a(void);
bool sim_get_button_b(void);

/**
 * Get current CV voltage level (0-255 ADC value).
 */
uint8_t sim_get_cv_voltage(void);

/**
 * Output state getter.
 */
bool sim_get_output(void);

/**
 * LED state control (for neopixel simulation).
 */
void sim_set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void sim_get_led(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * Get current simulation time.
 */
uint32_t sim_get_time(void);

/**
 * Reset simulation time to 0.
 */
void sim_reset_time(void);

/**
 * Check if the simulated watchdog has fired.
 * Returns true if wdt_enable() was called and more than 250ms
 * elapsed without a wdt_reset() call.
 */
bool sim_wdt_has_fired(void);

/**
 * Reset watchdog fired state (for test setup).
 */
void sim_wdt_clear_fired(void);

#endif /* GK_SIM_HAL_H */
