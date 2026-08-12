#ifndef GK_CONFIG_MODE_CONFIG_H
#define GK_CONFIG_MODE_CONFIG_H

#include "utility/progmem.h"
#include <stdint.h>

/**
 * @file mode_config.h
 * @brief PROGMEM lookup tables for mode configuration values
 *
 * Maps settings indices (stored in EEPROM) to actual runtime values.
 * All tables are stored in flash (PROGMEM) to minimize RAM usage.
 *
 * Usage:
 *   uint16_t pulse = pgm_read_word(&TRIGGER_PULSE_VALUES[settings->trigger_pulse_idx]);
 */

// =============================================================================
// Trigger Mode Configuration
// =============================================================================

/**
 * Trigger pulse durations in milliseconds.
 * Index: 0=10ms (default), 1=50ms, 2=100ms, 3=1ms
 */
static const uint16_t TRIGGER_PULSE_VALUES[] PROGMEM_ATTR = {10, 50, 100, 1};
#define TRIGGER_PULSE_COUNT 4

/**
 * Edge detection modes for trigger.
 * Index: 0=rising, 1=falling, 2=both
 */
#define TRIGGER_EDGE_RISING  0
#define TRIGGER_EDGE_FALLING 1
#define TRIGGER_EDGE_BOTH    2
#define TRIGGER_EDGE_COUNT   3

// =============================================================================
// Divide Mode Configuration
// =============================================================================

/**
 * Clock divider ratios.
 * Index: 0=/2 (default), 1=/4, 2=/8, 3=/24
 */
static const uint8_t DIVIDE_DIVISOR_VALUES[] PROGMEM_ATTR = {2, 4, 8, 24};
#define DIVIDE_DIVISOR_COUNT 4

// =============================================================================
// Cycle Mode Configuration
// =============================================================================

/**
 * Cycle periods in milliseconds (derived from BPM).
 * BPM to period: period_ms = 60000 / BPM
 * Index: 0=60BPM (1000ms, default), 1=80BPM (750ms), 2=100BPM (600ms),
 *        3=120BPM (500ms), 4=160BPM (375ms)
 * NOTE: This will be refactored per FDP for Cycle mode improvements.
 */
static const uint16_t CYCLE_PERIOD_VALUES[] PROGMEM_ATTR = {1000, 750, 600, 500, 375};
#define CYCLE_TEMPO_COUNT 5

/**
 * BPM display values (for UI feedback, not used in calculations).
 */
static const uint8_t CYCLE_BPM_VALUES[] PROGMEM_ATTR = {60, 80, 100, 120, 160};

// =============================================================================
// Toggle Mode Configuration
// =============================================================================

/**
 * Edge detection modes for toggle.
 * Index: 0=rising (default), 1=falling
 */
#define TOGGLE_EDGE_RISING  0
#define TOGGLE_EDGE_FALLING 1
#define TOGGLE_EDGE_COUNT   2

// =============================================================================
// Gate Mode Configuration
// =============================================================================

/**
 * Gate A button modes.
 * Index: 0=disabled (default), 1=manual trigger
 */
#define GATE_A_MODE_OFF     0
#define GATE_A_MODE_MANUAL  1
#define GATE_A_MODE_COUNT   2

// =============================================================================
// Settings Validation
// =============================================================================

/**
 * Validation limits for AppSettings fields (in struct order).
 * Used by app_init.c to validate loaded settings with a single loop.
 * Order must match AppSettings struct: mode, trigger_pulse_idx, trigger_edge,
 * divide_divisor_idx, cycle_tempo_idx, toggle_edge, gate_a_mode, reserved.
 * Use 0 for reserved field (no validation - any value valid).
 */
#define SETTINGS_FIELD_COUNT 8

#endif /* GK_CONFIG_MODE_CONFIG_H */
