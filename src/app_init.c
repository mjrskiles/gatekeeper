#include "app_init.h"
#include "config/mode_config.h"
#include "utility/delay.h"
#include "utility/progmem.h"
#include "core/states.h"

// Size of AppSettings struct for iteration
#define APP_SETTINGS_SIZE sizeof(AppSettings)

/**
 * Validation limits for AppSettings fields (in struct order).
 * Value of 0 means no validation (any value valid).
 */
static const uint8_t SETTINGS_LIMITS[] PROGMEM_ATTR = {
    MODE_COUNT,             // mode
    TRIGGER_PULSE_COUNT,    // trigger_pulse_idx
    TRIGGER_EDGE_COUNT,     // trigger_edge
    DIVIDE_DIVISOR_COUNT,   // divide_divisor_idx
    CYCLE_TEMPO_COUNT,      // cycle_tempo_idx
    TOGGLE_EDGE_COUNT,      // toggle_edge
    GATE_A_MODE_COUNT,      // gate_a_mode
    0,                      // reserved (no validation)
};

/**
 * Calculate XOR checksum over settings struct.
 */
static uint8_t calculate_checksum(const AppSettings *settings) {
    const uint8_t *data = (const uint8_t *)settings;
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * Provide visual feedback that factory reset is pending.
 * Blinks output LED while waiting for buttons to be held.
 */
static void reset_feedback_tick(void) {
    p_hal->toggle_pin(p_hal->sig_out_pin);
}

/**
 * Provide visual feedback that factory reset completed.
 * Uses sig_out_pin which drives output LED via buffer circuit.
 */
static void reset_complete_feedback(void) {
    // Output LED solid for confirmation
    p_hal->set_pin(p_hal->sig_out_pin);
    util_delay_ms(500);
    p_hal->clear_pin(p_hal->sig_out_pin);
}

/**
 * Provide visual feedback that defaults are being used.
 * Double-blink pattern on output LED (driven via sig_out_pin buffer).
 */
static void defaults_feedback(void) {
    for (uint8_t i = 0; i < DEFAULTS_BLINK_COUNT; i++) {
        p_hal->set_pin(p_hal->sig_out_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->sig_out_pin);
        util_delay_ms(100);
    }
    util_delay_ms(200);
    for (uint8_t i = 0; i < DEFAULTS_BLINK_COUNT; i++) {
        p_hal->set_pin(p_hal->sig_out_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->sig_out_pin);
        util_delay_ms(100);
    }
}

void app_init_get_defaults(AppSettings *settings) {
    if (!settings) return;

    settings->mode = MODE_GATE;
    settings->trigger_pulse_idx = 0;    // Default: 10ms pulse
    settings->trigger_edge = 0;         // Default: rising edge
    settings->divide_divisor_idx = 0;   // Default: /2
    settings->cycle_tempo_idx = 0;      // Default: 60 BPM (1Hz)
    settings->toggle_edge = 0;          // Default: rising edge
    settings->gate_a_mode = 0;          // Default: A button disabled
    settings->reserved = 0;
}

bool app_init_check_factory_reset(void) {
    // Sanity check: verify timer is actually advancing.
    // If Timer0 ISR isn't running, millis() won't increment and we'd infinite-loop.
    uint32_t t1 = p_hal->millis();
    util_delay_ms(10);
    uint32_t t2 = p_hal->millis();
    if (t2 <= t1) {
        // Timer is not advancing - abort to avoid infinite loop
        return false;
    }

    // Check if both buttons are currently pressed (active-low: pressed = LOW)
    if (p_hal->read_pin(p_hal->button_a_pin) ||
        p_hal->read_pin(p_hal->button_b_pin)) {
        return false;
    }

    // Both pressed - start counting with visual feedback
    uint32_t start_time = p_hal->millis();
    uint32_t last_blink = start_time;
    uint16_t iterations = 0;

    // Loop with both time-based and iteration-based exit conditions.
    // Iteration limit provides defense against timer failure.
    while ((p_hal->millis() - start_time) < APP_INIT_RESET_HOLD_MS &&
           iterations < APP_INIT_RESET_MAX_ITERATIONS) {
        // Blink LEDs for feedback
        if ((p_hal->millis() - last_blink) >= APP_INIT_RESET_BLINK_MS) {
            reset_feedback_tick();
            last_blink = p_hal->millis();
        }

        // If either button released, abort (active-low: released = HIGH)
        if (p_hal->read_pin(p_hal->button_a_pin) ||
            p_hal->read_pin(p_hal->button_b_pin)) {
            // Turn off output LED and return
            p_hal->clear_pin(p_hal->sig_out_pin);
            return false;
        }

        // Small delay to avoid busy-waiting too hard
        util_delay_ms(APP_INIT_RESET_POLL_MS);
        iterations++;
    }

    // Held long enough - show confirmation
    reset_complete_feedback();
    return true;
}

void app_init_clear_eeprom(void) {
    // Clear magic number to invalidate
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, 0xFFFF);
}

void app_init_save_settings(const AppSettings *settings) {
    if (!settings) return;

    // Write magic number
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);

    // Write schema version
    p_hal->eeprom_write_byte(EEPROM_SCHEMA_ADDR, SETTINGS_SCHEMA_VERSION);

    // Write settings struct byte by byte
    const uint8_t *data = (const uint8_t *)settings;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        p_hal->eeprom_write_byte(EEPROM_SETTINGS_ADDR + i, data[i]);
    }

    // Write checksum
    p_hal->eeprom_write_byte(EEPROM_CHECKSUM_ADDR, calculate_checksum(settings));
}

/**
 * Validate and load settings from EEPROM.
 *
 * @param settings Pointer to settings struct to populate
 * @return true if settings are valid and loaded, false otherwise
 */
static bool load_settings(AppSettings *settings) {
    // Level 1: Check magic number
    uint16_t magic = p_hal->eeprom_read_word(EEPROM_MAGIC_ADDR);
    if (magic != EEPROM_MAGIC_VALUE) {
        return false;
    }

    // Level 2: Check schema version
    uint8_t schema = p_hal->eeprom_read_byte(EEPROM_SCHEMA_ADDR);
    if (schema != SETTINGS_SCHEMA_VERSION) {
        // Future: could attempt migration here
        // For now, treat as invalid
        return false;
    }

    // Level 3: Read settings and verify checksum
    uint8_t *data = (uint8_t *)settings;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        data[i] = p_hal->eeprom_read_byte(EEPROM_SETTINGS_ADDR + i);
    }

    uint8_t stored_checksum = p_hal->eeprom_read_byte(EEPROM_CHECKSUM_ADDR);
    uint8_t calculated_checksum = calculate_checksum(settings);
    if (stored_checksum != calculated_checksum) {
        return false;
    }

    // Level 4: Range validation using PROGMEM limits table
    const uint8_t *fields = (const uint8_t *)settings;
    for (uint8_t i = 0; i < SETTINGS_FIELD_COUNT; i++) {
        uint8_t limit = PROGMEM_READ_BYTE(&SETTINGS_LIMITS[i]);
        if (limit > 0 && fields[i] >= limit) {
            return false;
        }
    }

    return true;
}

AppInitResult app_init_run(AppSettings *settings) {
    if (!settings) return APP_INIT_OK_DEFAULTS;

    // Check for factory reset (both buttons held)
    if (app_init_check_factory_reset()) {
        app_init_clear_eeprom();
        app_init_get_defaults(settings);
        app_init_save_settings(settings);

        // Verify EEPROM write succeeded by reading back magic number.
        // If verification fails, we still continue with defaults in RAM,
        // but next boot will also use defaults (which is correct behavior).
        uint16_t verify_magic = p_hal->eeprom_read_word(EEPROM_MAGIC_ADDR);
        if (verify_magic != EEPROM_MAGIC_VALUE) {
            // EEPROM write failed - signal error with rapid blink on output LED
            for (uint8_t i = 0; i < 10; i++) {
                p_hal->toggle_pin(p_hal->sig_out_pin);
                util_delay_ms(50);
            }
            p_hal->clear_pin(p_hal->sig_out_pin);
        }

        return APP_INIT_OK_FACTORY_RESET;
    }

    // Attempt to load settings from EEPROM
    if (load_settings(settings)) {
        return APP_INIT_OK;
    }

    // Validation failed - use defaults
    app_init_get_defaults(settings);
    defaults_feedback();
    return APP_INIT_OK_DEFAULTS;
}
