#ifndef GK_HARDWARE_HAL_INTERFACE_H
#define GK_HARDWARE_HAL_INTERFACE_H

#include <stdint.h>

/**
 * Hardware Abstraction Layer (HAL) Interface
 *
 * This interface decouples application logic from hardware-specific code,
 * enabling unit testing on the host machine without actual hardware.
 *
 * Architecture:
 * - A global pointer `p_hal` points to the active HAL implementation
 * - Production builds use the real HAL (hal.c) targeting ATtiny85
 * - Test builds swap in a mock HAL (mock_hal.c) with virtual time
 *
 * Usage in tests:
 *   p_hal points to mock_hal by default in test builds.
 *   Use p_hal->advance_time(ms) to simulate time passing.
 *   Use p_hal->reset_time() in test teardown for isolation.
 *
 * Why a global?
 *   On memory-constrained MCUs (512 bytes SRAM), passing HAL pointers
 *   to every function wastes stack space. A single global is the
 *   standard embedded pattern and works well with the mock swap approach.
 */
typedef struct HalInterface {
    // Pin configuration
    uint8_t  max_pin;                   // Maximum valid pin number (for validation)

    // Pin assignments (directly readable for init without function call overhead)
    uint8_t  button_a_pin;              // Primary button (menu/mode)
    uint8_t  button_b_pin;              // Secondary button (value/action)
    uint8_t  sig_out_pin;               // CV output (also drives output LED via buffer)

    // IO functions
    void     (*init)(void);
    void     (*set_pin)(uint8_t pin);
    void     (*clear_pin)(uint8_t pin);
    void     (*toggle_pin)(uint8_t pin);
    uint8_t  (*read_pin)(uint8_t pin);

    // Timer functions
    void     (*init_timer)(void);
    uint32_t (*millis)(void);
    void     (*delay_ms)(uint32_t ms);  // Blocking delay
    void     (*advance_time)(uint32_t ms);  // Test helper: manually advance time
    void     (*reset_time)(void);  // Test helper: reset time to 0

    // EEPROM functions
    uint8_t  (*eeprom_read_byte)(uint16_t addr);
    void     (*eeprom_write_byte)(uint16_t addr, uint8_t value);
    uint16_t (*eeprom_read_word)(uint16_t addr);
    void     (*eeprom_write_word)(uint16_t addr, uint16_t value);

    // ADC functions (for analog CV input per ADR-004)
    uint8_t  (*adc_read)(uint8_t channel);  // Read 8-bit ADC value (0-255)

    // Watchdog functions
    void     (*wdt_enable)(void);   // Enable watchdog with default timeout
    void     (*wdt_reset)(void);    // Feed the watchdog (call in main loop)
    void     (*wdt_disable)(void);  // Disable watchdog (use sparingly)
} HalInterface;

// Global pointer to the current HAL implementation.
// This pointer defaults to the production HAL, but tests can replace it with a mock.
extern HalInterface *p_hal;

#endif /* GK_HARDWARE_HAL_INTERFACE_H */