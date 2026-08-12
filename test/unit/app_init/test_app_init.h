/**
 * @file test_app_init.h
 * @brief Unit tests for the app initialization module
 *
 * Tests EEPROM settings persistence, validation, and defaults.
 *
 * Note: Factory reset timing tests are not included because app_init
 * uses util_delay_ms() which busy-waits on p_hal->millis(). In the mock
 * environment, millis() doesn't advance during busy-wait loops. Factory
 * reset functionality should be tested manually or in integration tests.
 */

#ifndef GK_TEST_APP_INIT_H
#define GK_TEST_APP_INIT_H

#include "unity.h"
#include "unity_fixture.h"
#include "app_init.h"
#include "hardware/hal_interface.h"
#include "mocks/mock_hal.h"

// Helper functions for active-low buttons
static void press_a(void) {
    p_hal->clear_pin(p_hal->button_a_pin);  // Active-low: press = LOW
}

static void press_b(void) {
    p_hal->clear_pin(p_hal->button_b_pin);
}

static void release_a(void) {
    p_hal->set_pin(p_hal->button_a_pin);    // Active-low: release = HIGH
}

static void release_b(void) {
    p_hal->set_pin(p_hal->button_b_pin);
}

TEST_GROUP(AppInitTests);

TEST_SETUP(AppInitTests) {
    p_hal->init();
    mock_eeprom_clear();  // Reset EEPROM to 0xFF (erased state)
}

TEST_TEAR_DOWN(AppInitTests) {
    p_hal->reset_time();
}

/**
 * Test app_init_get_defaults() sets correct initial values
 */
TEST(AppInitTests, TestGetDefaults) {
    AppSettings settings;
    // Initialize with garbage to verify it gets overwritten
    settings.mode = 0xFF;
    settings.trigger_pulse_idx = 0xFF;
    settings.trigger_edge = 0xFF;

    app_init_get_defaults(&settings);

    TEST_ASSERT_EQUAL(MODE_GATE, settings.mode);
    TEST_ASSERT_EQUAL(0, settings.trigger_pulse_idx);   // Default: 10ms
    TEST_ASSERT_EQUAL(0, settings.trigger_edge);        // Default: rising
    TEST_ASSERT_EQUAL(0, settings.divide_divisor_idx);  // Default: /2
    TEST_ASSERT_EQUAL(0, settings.cycle_tempo_idx);     // Default: 60 BPM
    TEST_ASSERT_EQUAL(0, settings.toggle_edge);         // Default: rising
    TEST_ASSERT_EQUAL(0, settings.gate_a_mode);         // Default: off
    TEST_ASSERT_EQUAL(0, settings.reserved);
}

/**
 * Test init with empty (erased) EEPROM returns APP_INIT_OK_DEFAULTS
 */
TEST(AppInitTests, TestInitWithEmptyEeprom) {
    AppSettings settings;
    // EEPROM is already 0xFF from setup

    AppInitResult result = app_init_run(&settings);

    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
    TEST_ASSERT_EQUAL(MODE_GATE, settings.mode);
}

/**
 * Test init with valid saved settings returns APP_INIT_OK
 */
TEST(AppInitTests, TestInitWithValidSettings) {
    // First, save valid settings
    AppSettings saved;
    saved.mode = MODE_TRIGGER;
    saved.trigger_pulse_idx = 1;      // 20ms
    saved.trigger_edge = 0;           // rising
    saved.divide_divisor_idx = 2;     // /8
    saved.cycle_tempo_idx = 3;        // 120 BPM
    saved.toggle_edge = 0;
    saved.gate_a_mode = 0;
    saved.reserved = 0;
    app_init_save_settings(&saved);

    // Now init and verify settings are loaded
    AppSettings loaded;
    AppInitResult result = app_init_run(&loaded);

    TEST_ASSERT_EQUAL(APP_INIT_OK, result);
    TEST_ASSERT_EQUAL(MODE_TRIGGER, loaded.mode);
    TEST_ASSERT_EQUAL(1, loaded.trigger_pulse_idx);
    TEST_ASSERT_EQUAL(0, loaded.trigger_edge);
    TEST_ASSERT_EQUAL(2, loaded.divide_divisor_idx);
    TEST_ASSERT_EQUAL(3, loaded.cycle_tempo_idx);
}

/**
 * Test init with invalid magic number falls back to defaults
 */
TEST(AppInitTests, TestInitWithInvalidMagic) {
    // Write wrong magic number
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, 0x1234);

    AppSettings settings;
    AppInitResult result = app_init_run(&settings);

    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
    TEST_ASSERT_EQUAL(MODE_GATE, settings.mode);
}

/**
 * Test init with invalid schema version falls back to defaults
 */
TEST(AppInitTests, TestInitWithInvalidSchema) {
    // Write correct magic but wrong schema
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    p_hal->eeprom_write_byte(EEPROM_SCHEMA_ADDR, SETTINGS_SCHEMA_VERSION + 1);

    AppSettings settings;
    AppInitResult result = app_init_run(&settings);

    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
    TEST_ASSERT_EQUAL(MODE_GATE, settings.mode);
}

/**
 * Test init with corrupted checksum falls back to defaults
 */
TEST(AppInitTests, TestInitWithInvalidChecksum) {
    // Save valid settings first
    AppSettings saved;
    app_init_get_defaults(&saved);
    app_init_save_settings(&saved);

    // Corrupt the checksum
    uint8_t stored_checksum = p_hal->eeprom_read_byte(EEPROM_CHECKSUM_ADDR);
    p_hal->eeprom_write_byte(EEPROM_CHECKSUM_ADDR, stored_checksum ^ 0xFF);

    AppSettings settings;
    AppInitResult result = app_init_run(&settings);

    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
}

/**
 * Test init with out-of-range mode value falls back to defaults
 */
TEST(AppInitTests, TestInitWithInvalidModeValue) {
    // Save settings with invalid mode (>= MODE_COUNT)
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    p_hal->eeprom_write_byte(EEPROM_SCHEMA_ADDR, SETTINGS_SCHEMA_VERSION);

    // Write settings manually with invalid mode
    uint8_t invalid_mode = MODE_COUNT;  // Out of range (first invalid value)
    p_hal->eeprom_write_byte(EEPROM_SETTINGS_ADDR, invalid_mode);
    // Write zeros for rest of settings
    for (int i = 1; i < 8; i++) {
        p_hal->eeprom_write_byte(EEPROM_SETTINGS_ADDR + i, 0);
    }
    // Calculate checksum for the invalid data
    uint8_t checksum = invalid_mode;  // XOR with zeros = invalid_mode
    p_hal->eeprom_write_byte(EEPROM_CHECKSUM_ADDR, checksum);

    AppSettings settings;
    AppInitResult result = app_init_run(&settings);

    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
    TEST_ASSERT_EQUAL(MODE_GATE, settings.mode);
}

/**
 * Test save and load round-trip for all modes
 */
TEST(AppInitTests, TestSaveAndLoadAllModes) {
    ModeState modes[] = {MODE_GATE, MODE_TRIGGER, MODE_TOGGLE, MODE_DIVIDE, MODE_CYCLE};

    for (int i = 0; i < MODE_COUNT; i++) {
        // Clear EEPROM between tests
        mock_eeprom_clear();

        AppSettings saved;
        app_init_get_defaults(&saved);
        saved.mode = modes[i];
        app_init_save_settings(&saved);

        AppSettings loaded;
        AppInitResult result = app_init_run(&loaded);

        TEST_ASSERT_EQUAL(APP_INIT_OK, result);
        TEST_ASSERT_EQUAL(modes[i], loaded.mode);
    }
}

/**
 * Test app_init_clear_eeprom() invalidates magic number
 */
TEST(AppInitTests, TestClearEeprom) {
    // First save valid settings
    AppSettings saved;
    app_init_get_defaults(&saved);
    app_init_save_settings(&saved);

    // Verify we can load them
    AppSettings verify;
    TEST_ASSERT_EQUAL(APP_INIT_OK, app_init_run(&verify));

    // Clear EEPROM
    app_init_clear_eeprom();

    // Verify magic is now invalid
    uint16_t magic = p_hal->eeprom_read_word(EEPROM_MAGIC_ADDR);
    TEST_ASSERT_EQUAL(0xFFFF, magic);

    // Verify init now falls back to defaults
    AppSettings settings;
    AppInitResult result = app_init_run(&settings);
    TEST_ASSERT_EQUAL(APP_INIT_OK_DEFAULTS, result);
}

/**
 * Test that factory reset check returns false when no buttons pressed
 * (Fast path - doesn't involve timing)
 */
TEST(AppInitTests, TestFactoryResetNotTriggeredWhenButtonsReleased) {
    // Both buttons are released (active-low: released = HIGH)
    // mock_hal_init already sets buttons HIGH, but be explicit
    release_a();
    release_b();

    bool result = app_init_check_factory_reset();

    TEST_ASSERT_FALSE(result);
}

/**
 * Test that factory reset check returns false when only one button pressed
 */
TEST(AppInitTests, TestFactoryResetNotTriggeredWithOneButton) {
    // Only button A pressed (active-low)
    press_a();
    release_b();

    bool result = app_init_check_factory_reset();
    TEST_ASSERT_FALSE(result);

    // Only button B pressed
    release_a();
    press_b();

    result = app_init_check_factory_reset();
    TEST_ASSERT_FALSE(result);
}

/**
 * Test EEPROM address constants are reasonable
 */
TEST(AppInitTests, TestEepromLayoutConstants) {
    // Verify EEPROM layout fits in ATtiny85's 512 bytes
    TEST_ASSERT_TRUE(EEPROM_CHECKSUM_ADDR < 512);

    // Verify settings struct size matches expectations
    TEST_ASSERT_EQUAL(8, sizeof(AppSettings));

    // Verify magic is at start
    TEST_ASSERT_EQUAL(0, EEPROM_MAGIC_ADDR);

    // Verify no overlap between magic, schema, settings, and checksum
    TEST_ASSERT_TRUE(EEPROM_SCHEMA_ADDR > EEPROM_MAGIC_ADDR + 1);
    TEST_ASSERT_TRUE(EEPROM_SETTINGS_ADDR > EEPROM_SCHEMA_ADDR);
    TEST_ASSERT_TRUE(EEPROM_CHECKSUM_ADDR >= EEPROM_SETTINGS_ADDR + sizeof(AppSettings));
}

/**
 * Test settings struct packing (important for EEPROM storage)
 */
TEST(AppInitTests, TestSettingsStructPacking) {
    AppSettings s;

    // Verify struct members are at expected offsets
    // This ensures EEPROM reads/writes align with struct layout
    uint8_t *base = (uint8_t *)&s;
    TEST_ASSERT_EQUAL_PTR(base + 0, &s.mode);
    TEST_ASSERT_EQUAL_PTR(base + 1, &s.trigger_pulse_idx);
    TEST_ASSERT_EQUAL_PTR(base + 2, &s.trigger_edge);
    TEST_ASSERT_EQUAL_PTR(base + 3, &s.divide_divisor_idx);
    TEST_ASSERT_EQUAL_PTR(base + 4, &s.cycle_tempo_idx);
    TEST_ASSERT_EQUAL_PTR(base + 5, &s.toggle_edge);
    TEST_ASSERT_EQUAL_PTR(base + 6, &s.gate_a_mode);
    TEST_ASSERT_EQUAL_PTR(base + 7, &s.reserved);
}

TEST_GROUP_RUNNER(AppInitTests) {
    RUN_TEST_CASE(AppInitTests, TestGetDefaults);
    RUN_TEST_CASE(AppInitTests, TestInitWithEmptyEeprom);
    RUN_TEST_CASE(AppInitTests, TestInitWithValidSettings);
    RUN_TEST_CASE(AppInitTests, TestInitWithInvalidMagic);
    RUN_TEST_CASE(AppInitTests, TestInitWithInvalidSchema);
    RUN_TEST_CASE(AppInitTests, TestInitWithInvalidChecksum);
    RUN_TEST_CASE(AppInitTests, TestInitWithInvalidModeValue);
    RUN_TEST_CASE(AppInitTests, TestSaveAndLoadAllModes);
    RUN_TEST_CASE(AppInitTests, TestClearEeprom);
    RUN_TEST_CASE(AppInitTests, TestFactoryResetNotTriggeredWhenButtonsReleased);
    RUN_TEST_CASE(AppInitTests, TestFactoryResetNotTriggeredWithOneButton);
    RUN_TEST_CASE(AppInitTests, TestEepromLayoutConstants);
    RUN_TEST_CASE(AppInitTests, TestSettingsStructPacking);
}

void RunAllAppInitTests(void) {
    RUN_TEST_GROUP(AppInitTests);
}

#endif /* GK_TEST_APP_INIT_H */
