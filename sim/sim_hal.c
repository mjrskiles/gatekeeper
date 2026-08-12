#include "sim_hal.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @file sim_hal.c
 * @brief x86 simulator HAL implementation
 *
 * Pure hardware abstraction - no display code.
 * Display is handled by renderers.
 */

// Simulated hardware state
#define SIM_NUM_PINS 8
static uint8_t pin_states[SIM_NUM_PINS] = {0};

// Simulated EEPROM
#define SIM_EEPROM_SIZE 512
static uint8_t sim_eeprom[SIM_EEPROM_SIZE];

// Time tracking
static uint32_t sim_time_ms = 0;

// LED state (for neopixel simulation)
static uint8_t led_r[SIM_NUM_LEDS] = {0};
static uint8_t led_g[SIM_NUM_LEDS] = {0};
static uint8_t led_b[SIM_NUM_LEDS] = {0};

// CV input voltage (0-255 ADC value, maps to 0-5V)
static uint8_t sim_cv_voltage = 0;

// ADC fault injection state (FDP-016)
static SimAdcMode adc_fault_mode = SIM_ADC_NORMAL;
static uint8_t adc_noise_amplitude = 10;  // Default +/- 10 ADC units

// EEPROM fault injection state (FDP-016 Phase 3)
static SimEepromMode eeprom_fault_mode = SIM_EEPROM_NORMAL;

// Watchdog simulation
#define SIM_WDT_TIMEOUT_MS 250
static bool wdt_enabled = false;
static uint32_t wdt_last_reset_time = 0;
static bool wdt_fired = false;

// Pin assignments (match mock_hal for consistency)
// Note: Neopixels are controlled via sim_neopixel.c, not GPIO
#define PIN_BUTTON_A    2
#define PIN_BUTTON_B    4
#define PIN_SIG_OUT     1   // Also drives output LED via buffer circuit

// Forward declarations
static void sim_hal_init(void);
static void sim_set_pin(uint8_t pin);
static void sim_clear_pin(uint8_t pin);
static void sim_toggle_pin(uint8_t pin);
static uint8_t sim_read_pin(uint8_t pin);
static void sim_init_timer(void);
static uint32_t sim_millis(void);
static void sim_delay_ms(uint32_t ms);
static void sim_advance_time(uint32_t ms);
void sim_reset_time(void);  // Public - used by input_source
static uint8_t sim_eeprom_read_byte(uint16_t addr);
static void sim_eeprom_write_byte(uint16_t addr, uint8_t value);
static uint16_t sim_eeprom_read_word(uint16_t addr);
static void sim_eeprom_write_word(uint16_t addr, uint16_t value);
static uint8_t sim_adc_read(uint8_t channel);
static void sim_wdt_enable(void);
static void sim_wdt_reset(void);
static void sim_wdt_disable(void);
static void check_watchdog(void);

// Global HAL pointer (defined in hal_interface.h as extern)
HalInterface *p_hal = NULL;

// The simulator HAL interface
static HalInterface sim_hal = {
    .max_pin            = SIM_NUM_PINS - 1,
    .button_a_pin       = PIN_BUTTON_A,
    .button_b_pin       = PIN_BUTTON_B,
    .sig_out_pin        = PIN_SIG_OUT,
    .init               = sim_hal_init,
    .set_pin            = sim_set_pin,
    .clear_pin          = sim_clear_pin,
    .toggle_pin         = sim_toggle_pin,
    .read_pin           = sim_read_pin,
    .init_timer         = sim_init_timer,
    .millis             = sim_millis,
    .delay_ms           = sim_delay_ms,
    .advance_time       = sim_advance_time,
    .reset_time         = sim_reset_time,
    .eeprom_read_byte   = sim_eeprom_read_byte,
    .eeprom_write_byte  = sim_eeprom_write_byte,
    .eeprom_read_word   = sim_eeprom_read_word,
    .eeprom_write_word  = sim_eeprom_write_word,
    .adc_read           = sim_adc_read,
    .wdt_enable         = sim_wdt_enable,
    .wdt_reset          = sim_wdt_reset,
    .wdt_disable        = sim_wdt_disable,
};

// =============================================================================
// HAL Implementation
// =============================================================================

static void sim_hal_init(void) {
    memset(pin_states, 0, sizeof(pin_states));
    // Button pins start HIGH (simulating internal pull-ups, active-low buttons)
    // Press = clear pin (LOW), Release = set pin (HIGH)
    pin_states[PIN_BUTTON_A] = 1;
    pin_states[PIN_BUTTON_B] = 1;

    memset(sim_eeprom, 0xFF, sizeof(sim_eeprom));
    sim_time_ms = 0;
}

static void sim_set_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    pin_states[pin] = 1;
}

static void sim_clear_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    pin_states[pin] = 0;
}

static void sim_toggle_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    pin_states[pin] = !pin_states[pin];
}

static uint8_t sim_read_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return 0;
    return pin_states[pin];
}

static void sim_init_timer(void) {
    // Nothing needed
}

static uint32_t sim_millis(void) {
    return sim_time_ms;
}

static void sim_delay_ms(uint32_t ms) {
    sim_time_ms += ms;
    check_watchdog();
}

static void sim_advance_time(uint32_t ms) {
    sim_time_ms += ms;
    check_watchdog();
}

void sim_reset_time(void) {
    sim_time_ms = 0;
}

static uint8_t sim_eeprom_read_byte(uint16_t addr) {
    if (addr >= SIM_EEPROM_SIZE) return 0xFF;

    // Apply fault injection (FDP-016 Phase 3)
    switch (eeprom_fault_mode) {
        case SIM_EEPROM_READ_FF:
            return 0xFF;

        case SIM_EEPROM_CORRUPT: {
            // Randomly flip one bit in the returned value
            uint8_t value = sim_eeprom[addr];
            uint8_t bit = rand() % 8;
            return value ^ (1 << bit);
        }

        case SIM_EEPROM_WRITE_FAIL:
        case SIM_EEPROM_NORMAL:
        default:
            return sim_eeprom[addr];
    }
}

static void sim_eeprom_write_byte(uint16_t addr, uint8_t value) {
    if (addr >= SIM_EEPROM_SIZE) return;

    // Apply fault injection (FDP-016 Phase 3)
    if (eeprom_fault_mode == SIM_EEPROM_WRITE_FAIL) {
        return;  // Silently fail
    }

    sim_eeprom[addr] = value;
}

static uint16_t sim_eeprom_read_word(uint16_t addr) {
    // Use byte-level functions to get fault injection
    uint8_t lo = sim_eeprom_read_byte(addr);
    uint8_t hi = sim_eeprom_read_byte(addr + 1);
    return lo | ((uint16_t)hi << 8);
}

static void sim_eeprom_write_word(uint16_t addr, uint16_t value) {
    // Use byte-level functions to get fault injection
    sim_eeprom_write_byte(addr, value & 0xFF);
    sim_eeprom_write_byte(addr + 1, (value >> 8) & 0xFF);
}

static uint8_t sim_adc_read(uint8_t channel) {
    // In simulator, channel 3 (CV input) returns the simulated CV voltage
    // Other channels return 0
    if (channel != 3) {
        return 0;
    }

    // Apply fault injection (FDP-016)
    switch (adc_fault_mode) {
        case SIM_ADC_TIMEOUT:
            // Match hardware timeout behavior: returns 128 (mid-scale)
            return 128;

        case SIM_ADC_STUCK_LOW:
            return 0;

        case SIM_ADC_STUCK_HIGH:
            return 255;

        case SIM_ADC_NOISY: {
            // Add random noise to the actual value
            int16_t noisy = (int16_t)sim_cv_voltage +
                ((rand() % (2 * adc_noise_amplitude + 1)) - adc_noise_amplitude);
            if (noisy < 0) noisy = 0;
            if (noisy > 255) noisy = 255;
            return (uint8_t)noisy;
        }

        case SIM_ADC_NORMAL:
        default:
            return sim_cv_voltage;
    }
}

// Watchdog simulation - checks if timeout exceeded
static void check_watchdog(void) {
    if (wdt_enabled && !wdt_fired) {
        uint32_t elapsed = sim_time_ms - wdt_last_reset_time;
        if (elapsed >= SIM_WDT_TIMEOUT_MS) {
            wdt_fired = true;
            fprintf(stderr, "\n*** WATCHDOG FIRED! ***\n");
            fprintf(stderr, "    Time since last wdt_reset: %u ms (timeout: %d ms)\n",
                    (unsigned)elapsed, SIM_WDT_TIMEOUT_MS);
            fprintf(stderr, "    Current time: %u ms\n", (unsigned)sim_time_ms);
            fprintf(stderr, "    This would reset the MCU and cause a boot loop!\n\n");
        }
    }
}

static void sim_wdt_enable(void) {
    wdt_enabled = true;
    wdt_last_reset_time = sim_time_ms;
    wdt_fired = false;
}

static void sim_wdt_reset(void) {
    if (wdt_enabled) {
        wdt_last_reset_time = sim_time_ms;
    }
}

static void sim_wdt_disable(void) {
    wdt_enabled = false;
}

// =============================================================================
// Public API
// =============================================================================

HalInterface* sim_get_hal(void) {
    return &sim_hal;
}

void sim_set_button_a(bool pressed) {
    // Active-low: pressed = LOW (0), released = HIGH (1)
    pin_states[PIN_BUTTON_A] = !pressed;
}

void sim_set_button_b(bool pressed) {
    // Active-low: pressed = LOW (0), released = HIGH (1)
    pin_states[PIN_BUTTON_B] = !pressed;
}

void sim_set_cv_voltage(uint8_t adc_value) {
    sim_cv_voltage = adc_value;
}

void sim_adjust_cv_voltage(int16_t delta) {
    int16_t new_value = (int16_t)sim_cv_voltage + delta;
    if (new_value < 0) new_value = 0;
    if (new_value > 255) new_value = 255;
    sim_cv_voltage = (uint8_t)new_value;
}

bool sim_get_button_a(void) {
    // Active-low: pin LOW = pressed (return true)
    return !pin_states[PIN_BUTTON_A];
}

bool sim_get_button_b(void) {
    // Active-low: pin LOW = pressed (return true)
    return !pin_states[PIN_BUTTON_B];
}

uint8_t sim_get_cv_voltage(void) {
    return sim_cv_voltage;
}

bool sim_get_output(void) {
    return pin_states[PIN_SIG_OUT];
}

void sim_set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= SIM_NUM_LEDS) return;
    led_r[index] = r;
    led_g[index] = g;
    led_b[index] = b;
}

void sim_get_led(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (index >= SIM_NUM_LEDS) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }
    if (r) *r = led_r[index];
    if (g) *g = led_g[index];
    if (b) *b = led_b[index];
}

uint32_t sim_get_time(void) {
    return sim_time_ms;
}

bool sim_wdt_has_fired(void) {
    return wdt_fired;
}

void sim_wdt_clear_fired(void) {
    wdt_fired = false;
}

// =============================================================================
// Fault Injection API (FDP-016)
// =============================================================================

void sim_adc_set_mode(SimAdcMode mode) {
    adc_fault_mode = mode;
}

void sim_adc_set_noise_amplitude(uint8_t amplitude) {
    adc_noise_amplitude = amplitude;
}

SimAdcMode sim_adc_get_mode(void) {
    return adc_fault_mode;
}

void sim_eeprom_set_mode(SimEepromMode mode) {
    eeprom_fault_mode = mode;
}

SimEepromMode sim_eeprom_get_mode(void) {
    return eeprom_fault_mode;
}
