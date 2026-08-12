#ifndef GK_APP_INIT_H
#define GK_APP_INIT_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"
#include "core/states.h"

/**
 * @file app_init.h
 * @brief Application initialization for Gatekeeper
 *
 * Handles startup tasks before entering the main application loop:
 * - Factory reset detection (both buttons held on startup)
 * - EEPROM settings validation and loading
 * - Graceful degradation to defaults on EEPROM errors
 *
 * Initialization sequence:
 * 1. Check for factory reset (both buttons held for APP_INIT_RESET_HOLD_MS)
 *    - If triggered: clear EEPROM, write defaults, continue
 * 2. Load settings from EEPROM
 * 3. Validate settings (magic number, schema version, checksum, ranges)
 *    - If invalid: use defaults, signal warning
 * 4. Return to main for RUN mode entry
 */

// Timing constants
#define APP_INIT_RESET_HOLD_MS       3000  // Hold time for factory reset
#define APP_INIT_RESET_POLL_MS         50  // Polling interval during reset check
#define APP_INIT_RESET_BLINK_MS       100  // LED blink rate during reset pending

// Visual feedback constants
#define DEFAULTS_BLINK_COUNT            2  // Number of blinks in defaults feedback pattern

// Safety limits (defense against timer failure)
#define APP_INIT_RESET_MAX_ITERATIONS   ((APP_INIT_RESET_HOLD_MS / APP_INIT_RESET_POLL_MS) + 20)

// EEPROM layout
#define EEPROM_MAGIC_ADDR           0x00    // 2 bytes: magic number
#define EEPROM_SCHEMA_ADDR          0x02    // 1 byte: schema version
#define EEPROM_SETTINGS_ADDR        0x03    // Settings struct starts here
#define EEPROM_CHECKSUM_ADDR        0x10    // 1 byte: XOR checksum of settings

// Magic number: "GK" in ASCII (0x474B)
#define EEPROM_MAGIC_VALUE          0x474B

// Schema version - increment when settings struct changes
// Version 2: Added per-mode configuration parameters
#define SETTINGS_SCHEMA_VERSION     2

/**
 * Initialization result codes
 */
typedef enum {
    APP_INIT_OK,                    // Normal init, settings loaded from EEPROM
    APP_INIT_OK_DEFAULTS,           // Initialized with defaults (EEPROM invalid/empty)
    APP_INIT_OK_FACTORY_RESET,      // Factory reset performed, using defaults
} AppInitResult;

/**
 * Application settings loaded during initialization
 *
 * This struct is stored in EEPROM. When changing this struct:
 * 1. Increment SETTINGS_SCHEMA_VERSION
 * 2. Update app_init_migrate_settings() if migration is possible
 * 3. Update EEPROM_CHECKSUM_ADDR if struct size changes
 *
 * Version 2 layout (8 bytes):
 * - Per-mode configuration indices that map to PROGMEM lookup tables
 * - See include/config/mode_config.h for value definitions
 */
typedef struct AppSettings {
    uint8_t mode;               // ModeState enum value (0-4)
    uint8_t trigger_pulse_idx;  // Trigger pulse length: 0=10ms, 1=20ms, 2=50ms, 3=1ms
    uint8_t trigger_edge;       // Trigger edge: 0=rising, 1=falling, 2=both
    uint8_t divide_divisor_idx; // Divide ratio: 0=/2, 1=/4, 2=/8, 3=/24
    uint8_t cycle_tempo_idx;    // Cycle tempo: 0=60, 1=80, 2=100, 3=120, 4=160 BPM
    uint8_t toggle_edge;        // Toggle edge: 0=rising, 1=falling
    uint8_t gate_a_mode;        // Gate A button: 0=off, 1=manual trigger
    uint8_t reserved;           // Future expansion (total: 8 bytes)
} __attribute__((packed)) AppSettings;

/**
 * Execute the initialization sequence.
 *
 * Checks for factory reset, loads/validates EEPROM settings, and populates
 * the settings struct. On any validation failure, defaults are used and
 * the appropriate result code is returned.
 *
 * @param settings  Pointer to struct that will be populated with loaded settings
 * @return          Result code indicating how initialization completed
 */
AppInitResult app_init_run(AppSettings *settings);

/**
 * Get default settings.
 *
 * Populates the settings struct with safe default values.
 * Used for factory reset and when EEPROM is invalid.
 *
 * @param settings  Pointer to struct to populate with defaults
 */
void app_init_get_defaults(AppSettings *settings);

/**
 * Save settings to EEPROM.
 *
 * Writes the settings struct to EEPROM with magic number, schema version,
 * and checksum. Uses eeprom_update_* functions to minimize write cycles.
 *
 * @param settings  Pointer to settings to save
 */
void app_init_save_settings(const AppSettings *settings);

/**
 * Check if factory reset is being requested.
 *
 * Monitors both buttons and returns true if held for APP_INIT_RESET_HOLD_MS.
 * Provides visual feedback (blinking LEDs) while waiting.
 *
 * @return true if factory reset was requested, false otherwise
 */
bool app_init_check_factory_reset(void);

/**
 * Clear all EEPROM settings.
 *
 * Erases the magic number and settings area, forcing next init to use defaults.
 */
void app_init_clear_eeprom(void);

#endif /* GK_APP_INIT_H */
