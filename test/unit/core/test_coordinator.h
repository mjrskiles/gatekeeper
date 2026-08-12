#ifndef GK_TEST_COORDINATOR_H
#define GK_TEST_COORDINATOR_H

#include "unity.h"
#include "unity_fixture.h"
#include "core/coordinator.h"
#include "events/events.h"
#include "hardware/hal_interface.h"
#include "mocks/mock_hal.h"

/**
 * @file test_coordinator.h
 * @brief Unit tests for Coordinator module
 *
 * Tests focus on:
 * - Menu toggle gesture (EVT_MENU_TOGGLE enters and exits menu)
 * - Menu timeout reset on button activity
 * - Mode change gesture (EVT_MODE_NEXT)
 * - Preventing double-triggering on button release after compound gesture
 */

static Coordinator coord;
static AppSettings settings;

TEST_GROUP(CoordinatorTests);

TEST_SETUP(CoordinatorTests) {
    mock_hal_init();
    mock_eeprom_clear();
    reset_mock_time();

    // Initialize with default settings
    app_init_get_defaults(&settings);
    coordinator_init(&coord, &settings);
    coordinator_start(&coord);
}

TEST_TEAR_DOWN(CoordinatorTests) {
    reset_mock_time();
}

// =============================================================================
// Helper functions
// =============================================================================

/**
 * Simulate pressing a button (active-low: press = LOW)
 */
static void press_button_a(void) {
    mock_clear_pin(p_hal->button_a_pin);
}

static void release_button_a(void) {
    mock_set_pin(p_hal->button_a_pin);
}

static void press_button_b(void) {
    mock_clear_pin(p_hal->button_b_pin);
}

static void release_button_b(void) {
    mock_set_pin(p_hal->button_b_pin);
}

/**
 * Run coordinator update loop for specified milliseconds
 */
static void run_for_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        coordinator_update(&coord);
        advance_mock_time(1);
    }
}

/**
 * Perform menu toggle gesture: A hold + B hold (A pressed first)
 * Returns with both buttons still held
 */
static void do_menu_toggle_gesture(void) {
    // Press A first
    press_button_a();
    run_for_ms(100);

    // Press B while A is held
    press_button_b();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 50);  // Wait for B to reach hold threshold
}

/**
 * Perform mode next gesture: A hold (solo) → release
 * Returns with A released
 */
static void do_mode_next_gesture(void) {
    // Press A (solo, no B)
    press_button_a();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 50);  // Wait for A to reach hold threshold

    // Release A to trigger mode change
    release_button_a();
    run_for_ms(50);
}

/**
 * Perform menu exit gesture: A hold (solo) - exits on hold threshold, no release needed
 */
static void do_menu_exit_gesture(void) {
    // Press A (solo, no B)
    press_button_a();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 50);  // Wait for A to reach hold threshold - menu exits here

    // Release A
    release_button_a();
    run_for_ms(50);
}

// =============================================================================
// Menu Toggle Tests
// =============================================================================

TEST(CoordinatorTests, TestMenuToggleEntersMenu) {
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));

    do_menu_toggle_gesture();

    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestMenuExitsOnAHold) {
    // Enter menu first
    do_menu_toggle_gesture();
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Release both buttons
    release_button_a();
    release_button_b();
    run_for_ms(100);

    // Still in menu after release
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Exit menu with A:hold (solo) - exits immediately on hold threshold
    do_menu_exit_gesture();

    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestMenuDoesNotExitOnButtonRelease) {
    // Enter menu
    do_menu_toggle_gesture();
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Release A (this used to incorrectly exit menu)
    release_button_a();
    run_for_ms(100);

    // Should still be in menu
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Release B
    release_button_b();
    run_for_ms(100);

    // Should still be in menu
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestMenuExitsOnAHoldButNotBHold) {
    // Enter menu
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Hold just B - should NOT exit menu (B:hold has no effect)
    press_button_b();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
    release_button_b();
    run_for_ms(100);

    // Hold just A - SHOULD exit menu (A:hold solo = exit)
    press_button_a();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 100);
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));
}

// =============================================================================
// Menu Timeout Tests
// =============================================================================

TEST(CoordinatorTests, TestMenuTimeoutExitsMenu) {
    // Enter menu
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Wait for timeout (60 seconds)
    run_for_ms(MENU_TIMEOUT_MS + 100);

    // Should have exited menu
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestMenuTimeoutResetsOnButtonPress) {
    // Enter menu
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Wait almost to timeout
    run_for_ms(MENU_TIMEOUT_MS - 1000);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Press and release A (this should reset timeout)
    press_button_a();
    run_for_ms(50);
    release_button_a();
    run_for_ms(50);

    // Wait almost to timeout again
    run_for_ms(MENU_TIMEOUT_MS - 1000);

    // Should still be in menu (timeout was reset)
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Now wait for full timeout
    run_for_ms(2000);
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestMenuTimeoutResetsOnButtonHold) {
    // Enter menu
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);

    // Wait almost to timeout
    run_for_ms(MENU_TIMEOUT_MS - 1000);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Press and hold B (generates EVT_B_PRESS, then EVT_B_HOLD)
    press_button_b();
    run_for_ms(EP_HOLD_THRESHOLD_MS + 100);
    release_button_b();
    run_for_ms(50);

    // Wait almost to timeout again
    run_for_ms(MENU_TIMEOUT_MS - 1000);

    // Should still be in menu
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
}

// =============================================================================
// Mode Change Tests
// =============================================================================

TEST(CoordinatorTests, TestModeNextChangesMode) {
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));

    // A:hold (solo) → release changes mode
    do_mode_next_gesture();

    TEST_ASSERT_EQUAL(MODE_TRIGGER, coordinator_get_mode(&coord));

    // Do again
    do_mode_next_gesture();
    TEST_ASSERT_EQUAL(MODE_TOGGLE, coordinator_get_mode(&coord));
}

TEST(CoordinatorTests, TestModeNextWrapsAround) {
    // Cycle through all modes (do_mode_next_gesture releases A at the end)
    for (int i = 0; i < MODE_COUNT; i++) {
        do_mode_next_gesture();
    }

    // Should wrap back to GATE
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));
}

TEST(CoordinatorTests, TestModeChangeDoesNotAffectOutput) {
    // In gate mode, press B (primary button) to set output high
    press_button_b();
    run_for_ms(50);
    coordinator_update(&coord);
    TEST_ASSERT_TRUE(coordinator_get_output(&coord));
    release_button_b();
    run_for_ms(50);

    // Change mode (A:hold solo → release)
    do_mode_next_gesture();

    // Output should be determined by new mode's initial state
    // (Trigger mode starts with output low)
    TEST_ASSERT_FALSE(coordinator_get_output(&coord));
}

// =============================================================================
// Compound Gesture Non-Interference Tests
// =============================================================================

TEST(CoordinatorTests, TestMenuGestureDoesNotTriggerModeChange) {
    ModeState initial_mode = coordinator_get_mode(&coord);

    // Menu gesture: A first, then B
    do_menu_toggle_gesture();

    // Mode should not have changed
    TEST_ASSERT_EQUAL(initial_mode, coordinator_get_mode(&coord));

    // But we should be in menu
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
}

TEST(CoordinatorTests, TestModeGestureDoesNotEnterMenu) {
    // Mode gesture: A:hold (solo) → release
    do_mode_next_gesture();

    // Should still be in perform mode
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));

    // But mode should have changed
    TEST_ASSERT_EQUAL(MODE_TRIGGER, coordinator_get_mode(&coord));
}

TEST(CoordinatorTests, TestMenuExitDoesNotChangeMode) {
    // Enter menu
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));

    // Exit menu with A:hold
    do_menu_exit_gesture();

    // Should be back in perform mode
    TEST_ASSERT_EQUAL(TOP_PERFORM, coordinator_get_top_state(&coord));

    // Mode should NOT have changed (still GATE)
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));
}

// =============================================================================
// Menu Value Cycling Tests
// =============================================================================

TEST(CoordinatorTests, TestMenuValueCyclesTriggerPulse) {
    // Enter menu and go to trigger pulse page
    coordinator_set_mode(&coord, MODE_TRIGGER);
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(TOP_MENU, coordinator_get_top_state(&coord));

    // Navigate to trigger pulse page (A tap advances page)
    press_button_a();
    run_for_ms(50);
    release_button_a();
    run_for_ms(50);
    TEST_ASSERT_EQUAL(PAGE_TRIGGER_PULSE_LEN, coordinator_get_page(&coord));

    // Initial value is default (index 0 = 10ms)
    TEST_ASSERT_EQUAL(0, settings.trigger_pulse_idx);

    // B tap cycles value
    press_button_b();
    run_for_ms(50);
    release_button_b();
    run_for_ms(50);

    // Value should have cycled (0 -> 1)
    TEST_ASSERT_EQUAL(1, settings.trigger_pulse_idx);
}

TEST(CoordinatorTests, TestMenuValueWrapsAround) {
    // Enter menu at divide page
    coordinator_set_mode(&coord, MODE_DIVIDE);
    do_menu_toggle_gesture();
    release_button_a();
    release_button_b();
    run_for_ms(100);
    TEST_ASSERT_EQUAL(PAGE_DIVIDE_DIVISOR, coordinator_get_page(&coord));

    // Initial value is 0 (/2)
    TEST_ASSERT_EQUAL(0, settings.divide_divisor_idx);

    // Cycle through all 4 values (0->1->2->3->0)
    for (int i = 0; i < 4; i++) {
        press_button_b();
        run_for_ms(50);
        release_button_b();
        run_for_ms(50);
    }

    // Should wrap back to 0
    TEST_ASSERT_EQUAL(0, settings.divide_divisor_idx);
}

// =============================================================================
// A Button Manual Trigger Tests
// =============================================================================

TEST(CoordinatorTests, TestGateAButtonDisabledByDefault) {
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));
    TEST_ASSERT_EQUAL(0, settings.gate_a_mode);  // Disabled by default

    // Press only A
    press_button_a();
    run_for_ms(50);

    // Output should still be false (A is disabled)
    TEST_ASSERT_FALSE(coordinator_get_output(&coord));

    release_button_a();
}

TEST(CoordinatorTests, TestGateAButtonEnabledTriggersOutput) {
    TEST_ASSERT_EQUAL(MODE_GATE, coordinator_get_mode(&coord));

    // Enable A button mode
    settings.gate_a_mode = 1;  // GATE_A_MODE_MANUAL

    // Press only A
    press_button_a();
    run_for_ms(50);

    // Output should be true (A triggers in gate mode)
    TEST_ASSERT_TRUE(coordinator_get_output(&coord));

    release_button_a();
    run_for_ms(50);

    // Output should be false again
    TEST_ASSERT_FALSE(coordinator_get_output(&coord));
}

TEST(CoordinatorTests, TestGateAButtonOnlyWorksInGateMode) {
    // Enable A button mode
    settings.gate_a_mode = 1;

    // Switch to Trigger mode
    coordinator_set_mode(&coord, MODE_TRIGGER);
    TEST_ASSERT_EQUAL(MODE_TRIGGER, coordinator_get_mode(&coord));

    // Press only A (should NOT trigger in Trigger mode)
    press_button_a();
    run_for_ms(50);

    // Output should be false (A only works in Gate mode)
    TEST_ASSERT_FALSE(coordinator_get_output(&coord));

    release_button_a();
}

// =============================================================================
// Test Runner
// =============================================================================

TEST_GROUP_RUNNER(CoordinatorTests) {
    // Menu toggle tests
    RUN_TEST_CASE(CoordinatorTests, TestMenuToggleEntersMenu);
    RUN_TEST_CASE(CoordinatorTests, TestMenuExitsOnAHold);
    RUN_TEST_CASE(CoordinatorTests, TestMenuDoesNotExitOnButtonRelease);
    RUN_TEST_CASE(CoordinatorTests, TestMenuExitsOnAHoldButNotBHold);

    // Menu timeout tests
    RUN_TEST_CASE(CoordinatorTests, TestMenuTimeoutExitsMenu);
    RUN_TEST_CASE(CoordinatorTests, TestMenuTimeoutResetsOnButtonPress);
    RUN_TEST_CASE(CoordinatorTests, TestMenuTimeoutResetsOnButtonHold);

    // Mode change tests
    RUN_TEST_CASE(CoordinatorTests, TestModeNextChangesMode);
    RUN_TEST_CASE(CoordinatorTests, TestModeNextWrapsAround);
    RUN_TEST_CASE(CoordinatorTests, TestModeChangeDoesNotAffectOutput);

    // Non-interference tests
    RUN_TEST_CASE(CoordinatorTests, TestMenuGestureDoesNotTriggerModeChange);
    RUN_TEST_CASE(CoordinatorTests, TestModeGestureDoesNotEnterMenu);
    RUN_TEST_CASE(CoordinatorTests, TestMenuExitDoesNotChangeMode);

    // Menu value cycling tests
    RUN_TEST_CASE(CoordinatorTests, TestMenuValueCyclesTriggerPulse);
    RUN_TEST_CASE(CoordinatorTests, TestMenuValueWrapsAround);

    // A button manual trigger tests
    RUN_TEST_CASE(CoordinatorTests, TestGateAButtonDisabledByDefault);
    RUN_TEST_CASE(CoordinatorTests, TestGateAButtonEnabledTriggersOutput);
    RUN_TEST_CASE(CoordinatorTests, TestGateAButtonOnlyWorksInGateMode);
}

void RunAllCoordinatorTests(void) {
    RUN_TEST_GROUP(CoordinatorTests);
}

#endif /* GK_TEST_COORDINATOR_H */
