#ifndef TEST_LED_FEEDBACK_H
#define TEST_LED_FEEDBACK_H

#include "unity.h"
#include "unity_fixture.h"
#include "output/led_feedback.h"
#include "output/neopixel.h"
#include "mocks/mock_neopixel.h"
#include "core/states.h"

/**
 * @file test_led_feedback.h
 * @brief Unit tests for LED feedback controller
 */

TEST_GROUP(LEDFeedbackTests);

static LEDFeedbackController ctrl;

TEST_SETUP(LEDFeedbackTests) {
    mock_neopixel_reset();
    led_feedback_init(&ctrl);
}

TEST_TEAR_DOWN(LEDFeedbackTests) {
}

TEST(LEDFeedbackTests, TestInit) {
    TEST_ASSERT_FALSE(ctrl.in_menu);
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.current_mode);
}

TEST(LEDFeedbackTests, TestModeColors) {
    // Gate mode - should be green
    NeopixelColor gate = led_feedback_get_mode_color(MODE_GATE);
    TEST_ASSERT_EQUAL_UINT8(0, gate.r);
    TEST_ASSERT_EQUAL_UINT8(255, gate.g);
    TEST_ASSERT_EQUAL_UINT8(0, gate.b);

    // Trigger mode - should be cyan-ish
    NeopixelColor trigger = led_feedback_get_mode_color(MODE_TRIGGER);
    TEST_ASSERT_EQUAL_UINT8(0, trigger.r);
    TEST_ASSERT_TRUE(trigger.g > 0);
    TEST_ASSERT_TRUE(trigger.b > 0);

    // Invalid mode - should be black
    NeopixelColor invalid = led_feedback_get_mode_color(99);
    TEST_ASSERT_EQUAL_UINT8(0, invalid.r);
    TEST_ASSERT_EQUAL_UINT8(0, invalid.g);
    TEST_ASSERT_EQUAL_UINT8(0, invalid.b);
}

TEST(LEDFeedbackTests, TestSetModeUpdatesColor) {
    led_feedback_set_mode(&ctrl, MODE_TOGGLE);

    // Update to apply the color (feedback must include current_mode to avoid reset)
    LEDFeedback fb = {
        .current_mode = MODE_TOGGLE,
        .in_menu = false
    };
    led_feedback_update(&ctrl, &fb, 0);

    // Mode LED should now be toggle color (orange)
    NeopixelColor expected = led_feedback_get_mode_color(MODE_TOGGLE);
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE,
                                                expected.r, expected.g, expected.b));
}

TEST(LEDFeedbackTests, TestActivityLED) {
    led_feedback_set_mode(&ctrl, MODE_GATE);

    // Activity on (white at full brightness)
    LEDFeedback fb = {
        .mode_r = 0, .mode_g = 255, .mode_b = 0,
        .activity_r = 255, .activity_g = 255, .activity_b = 255,
        .activity_brightness = 255,
        .current_mode = MODE_GATE,
        .in_menu = false
    };

    led_feedback_update(&ctrl, &fb, 0);

    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_ACTIVITY, 255, 255, 255));
}

TEST(LEDFeedbackTests, TestActivityOff) {
    led_feedback_set_mode(&ctrl, MODE_GATE);

    LEDFeedback fb = {
        .activity_r = 255, .activity_g = 255, .activity_b = 255,
        .activity_brightness = 0,  // Off
        .current_mode = MODE_GATE,
        .in_menu = false
    };

    led_feedback_update(&ctrl, &fb, 0);

    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_ACTIVITY, 0, 0, 0));
}

TEST(LEDFeedbackTests, TestMenuEnterFirstPageBlinks) {
    // First page of group should blink (clearly different from solid perform mode)
    led_feedback_enter_menu(&ctrl, PAGE_GATE_CV);

    TEST_ASSERT_TRUE(ctrl.in_menu);
    TEST_ASSERT_EQUAL(ANIM_BLINK, ctrl.mode_anim.type);
}

TEST(LEDFeedbackTests, TestMenuEnterSecondPageGlows) {
    // Second page of group should glow to differentiate from first
    led_feedback_enter_menu(&ctrl, PAGE_TRIGGER_PULSE_LEN);

    TEST_ASSERT_TRUE(ctrl.in_menu);
    TEST_ASSERT_EQUAL(ANIM_GLOW, ctrl.mode_anim.type);
}

TEST(LEDFeedbackTests, TestMenuExitRestoresMode) {
    led_feedback_set_mode(&ctrl, MODE_DIVIDE);
    led_feedback_enter_menu(&ctrl, PAGE_DIVIDE_DIVISOR);

    led_feedback_exit_menu(&ctrl);

    TEST_ASSERT_FALSE(ctrl.in_menu);
    TEST_ASSERT_EQUAL(ANIM_NONE, ctrl.mode_anim.type);

    // Mode color should be restored
    NeopixelColor expected = led_feedback_get_mode_color(MODE_DIVIDE);
    TEST_ASSERT_EQUAL_UINT8(expected.r, ctrl.mode_anim.base_color.r);
    TEST_ASSERT_EQUAL_UINT8(expected.g, ctrl.mode_anim.base_color.g);
    TEST_ASSERT_EQUAL_UINT8(expected.b, ctrl.mode_anim.base_color.b);
}

TEST(LEDFeedbackTests, TestPageColors) {
    // Each page should have a color
    for (int page = 0; page < PAGE_COUNT; page++) {
        NeopixelColor color = led_feedback_get_page_color(page);
        // At least one component should be non-zero for valid pages
        TEST_ASSERT_TRUE(color.r > 0 || color.g > 0 || color.b > 0);
    }
}

TEST(LEDFeedbackTests, TestSetPageUpdatesColor) {
    led_feedback_enter_menu(&ctrl, PAGE_GATE_CV);

    led_feedback_set_page(&ctrl, PAGE_CYCLE_PATTERN);

    NeopixelColor expected = led_feedback_get_page_color(PAGE_CYCLE_PATTERN);
    TEST_ASSERT_EQUAL_UINT8(expected.r, ctrl.mode_anim.base_color.r);
    TEST_ASSERT_EQUAL_UINT8(expected.g, ctrl.mode_anim.base_color.g);
    TEST_ASSERT_EQUAL_UINT8(expected.b, ctrl.mode_anim.base_color.b);
}

TEST(LEDFeedbackTests, TestNullSafety) {
    // These should not crash
    led_feedback_init(NULL);
    led_feedback_update(NULL, NULL, 0);
    led_feedback_set_mode(NULL, MODE_GATE);
    led_feedback_enter_menu(NULL, 0);
    led_feedback_exit_menu(NULL);
    led_feedback_set_page(NULL, 0);
    led_feedback_flash(NULL, 255, 255, 255);
}

TEST_GROUP_RUNNER(LEDFeedbackTests) {
    RUN_TEST_CASE(LEDFeedbackTests, TestInit);
    RUN_TEST_CASE(LEDFeedbackTests, TestModeColors);
    RUN_TEST_CASE(LEDFeedbackTests, TestSetModeUpdatesColor);
    RUN_TEST_CASE(LEDFeedbackTests, TestActivityLED);
    RUN_TEST_CASE(LEDFeedbackTests, TestActivityOff);
    RUN_TEST_CASE(LEDFeedbackTests, TestMenuEnterFirstPageBlinks);
    RUN_TEST_CASE(LEDFeedbackTests, TestMenuEnterSecondPageGlows);
    RUN_TEST_CASE(LEDFeedbackTests, TestMenuExitRestoresMode);
    RUN_TEST_CASE(LEDFeedbackTests, TestPageColors);
    RUN_TEST_CASE(LEDFeedbackTests, TestSetPageUpdatesColor);
    RUN_TEST_CASE(LEDFeedbackTests, TestNullSafety);
}

void RunAllLEDFeedbackTests(void) {
    RUN_TEST_GROUP(LEDFeedbackTests);
}

#endif /* TEST_LED_FEEDBACK_H */
