#include "output/led_feedback.h"
#include "output/neopixel.h"
#include "output/led_animation.h"
#include "core/states.h"

/**
 * @file led_feedback.c
 * @brief High-level LED feedback controller implementation
 */

// Mode colors (indexed by ModeState)
static const NeopixelColor mode_colors[] = {
    {LED_COLOR_GATE_R,    LED_COLOR_GATE_G,    LED_COLOR_GATE_B},      // MODE_GATE - Green
    {LED_COLOR_TRIGGER_R, LED_COLOR_TRIGGER_G, LED_COLOR_TRIGGER_B},  // MODE_TRIGGER - Cyan
    {LED_COLOR_TOGGLE_R,  LED_COLOR_TOGGLE_G,  LED_COLOR_TOGGLE_B},   // MODE_TOGGLE - Orange
    {LED_COLOR_DIVIDE_R,  LED_COLOR_DIVIDE_G,  LED_COLOR_DIVIDE_B},   // MODE_DIVIDE - Magenta
    {LED_COLOR_CYCLE_R,   LED_COLOR_CYCLE_G,   LED_COLOR_CYCLE_B},    // MODE_CYCLE - Yellow
};

// Global settings color (white)
static const NeopixelColor global_color = {255, 255, 255};

// Page to mode mapping (which mode's color to use for each page)
// Special value MODE_COUNT means "global" (use global_color)
static const uint8_t page_mode_map[] = {
    MODE_GATE,      // PAGE_GATE_CV
    MODE_TRIGGER,   // PAGE_TRIGGER_BEHAVIOR
    MODE_TRIGGER,   // PAGE_TRIGGER_PULSE_LEN
    MODE_TOGGLE,    // PAGE_TOGGLE_BEHAVIOR
    MODE_DIVIDE,    // PAGE_DIVIDE_DIVISOR
    MODE_CYCLE,     // PAGE_CYCLE_PATTERN
    MODE_COUNT,     // PAGE_CV_GLOBAL (global)
    MODE_COUNT,     // PAGE_MENU_TIMEOUT (global)
};

// Page animation type: 0 = blink (first page), 1 = glow (second page)
// Blink is default to clearly differentiate from solid perform mode
static const uint8_t page_anim_type[] = {
    0,  // PAGE_GATE_CV - first gate page (blink)
    0,  // PAGE_TRIGGER_BEHAVIOR - first trigger page (blink)
    1,  // PAGE_TRIGGER_PULSE_LEN - second trigger page (glow)
    0,  // PAGE_TOGGLE_BEHAVIOR - first toggle page (blink)
    0,  // PAGE_DIVIDE_DIVISOR - first divide page (blink)
    0,  // PAGE_CYCLE_PATTERN - first cycle page (blink)
    0,  // PAGE_CV_GLOBAL - first global page (blink)
    1,  // PAGE_MENU_TIMEOUT - second global page (glow)
};

void led_feedback_init(LEDFeedbackController *ctrl) {
    if (!ctrl) return;

    neopixel_init();

    led_animation_init(&ctrl->mode_anim);
    led_animation_init(&ctrl->activity_anim);

    ctrl->in_menu = false;
    ctrl->current_mode = 0;
    ctrl->current_page = 0;
    ctrl->last_setting_value = 0xFF;  // Invalid value to force initial set

    // Set initial mode color
    led_feedback_set_mode(ctrl, 0);
}

void led_feedback_update(LEDFeedbackController *ctrl,
                         const LEDFeedback *feedback,
                         uint32_t current_time) {
    if (!ctrl || !feedback) return;

    // Handle menu state transitions
    if (feedback->in_menu && !ctrl->in_menu) {
        // Entering menu
        led_feedback_enter_menu(ctrl, feedback->current_page);
    } else if (!feedback->in_menu && ctrl->in_menu) {
        // Exiting menu
        led_feedback_exit_menu(ctrl);
    }

    // Handle mode changes (when not in menu)
    if (!feedback->in_menu && feedback->current_mode != ctrl->current_mode) {
        led_feedback_set_mode(ctrl, feedback->current_mode);
    }

    // Handle page changes (when in menu)
    if (feedback->in_menu && feedback->current_page != ctrl->current_page) {
        led_feedback_set_page(ctrl, feedback->current_page);
    }

    if (!ctrl->in_menu) {
        // In perform mode: use feedback from mode handler

        // Mode LED: show mode color (already set by led_feedback_set_mode)
        led_animation_update(&ctrl->mode_anim, LED_MODE, current_time);

        // Activity LED: show output state with brightness from feedback
        NeopixelColor activity_color = {
            feedback->activity_r,
            feedback->activity_g,
            feedback->activity_b
        };

        if (feedback->activity_brightness == 255) {
            // Full on - static color
            led_animation_set_static(&ctrl->activity_anim, activity_color);
        } else if (feedback->activity_brightness == 0) {
            // Off
            led_animation_set_static(&ctrl->activity_anim, (NeopixelColor){0, 0, 0});
        } else {
            // Partial brightness (used by Cycle mode for pulsing)
            NeopixelColor scaled = led_color_scale(activity_color,
                                                   feedback->activity_brightness);
            led_animation_set_static(&ctrl->activity_anim, scaled);
        }

        led_animation_update(&ctrl->activity_anim, LED_ACTIVITY, current_time);
    } else {
        // In menu mode: LED X shows page, LED Y shows value

        // Update mode LED animation (solid or blink based on page type)
        led_animation_update(&ctrl->mode_anim, LED_MODE, current_time);

        // LED Y shows setting value as behavior (per flowchart):
        // 0: OFF, 1: ON, 2: BLINK, 3: GLOW
        // Color matches page color for visual consistency
        // Only update animation when value changes to avoid resetting animation state
        if (feedback->setting_value != ctrl->last_setting_value) {
            NeopixelColor page_color = led_feedback_get_page_color(ctrl->current_page);
            ctrl->last_setting_value = feedback->setting_value;

            switch (feedback->setting_value) {
                case 0:
                    // Value 0: LED Y OFF
                    led_animation_set_static(&ctrl->activity_anim, (NeopixelColor){0, 0, 0});
                    break;
                case 1:
                    // Value 1: LED Y ON (solid)
                    led_animation_set_static(&ctrl->activity_anim, page_color);
                    break;
                case 2:
                    // Value 2: LED Y BLINKING
                    led_animation_set(&ctrl->activity_anim, ANIM_BLINK, page_color, ANIM_BLINK_PERIOD_MS);
                    break;
                case 3:
                default:
                    // Value 3+: LED Y GLOWING
                    led_animation_set(&ctrl->activity_anim, ANIM_GLOW, page_color, ANIM_GLOW_PERIOD_MS);
                    break;
            }
        }
        led_animation_update(&ctrl->activity_anim, LED_ACTIVITY, current_time);
    }

    // Flush changes to LEDs (only if dirty)
    neopixel_flush();
}

void led_feedback_set_mode(LEDFeedbackController *ctrl, uint8_t mode) {
    if (!ctrl) return;
    if (mode >= MODE_COUNT) mode = 0;

    ctrl->current_mode = mode;

    if (!ctrl->in_menu) {
        // Static mode color in perform mode
        NeopixelColor color = led_feedback_get_mode_color(mode);
        led_animation_set_static(&ctrl->mode_anim, color);
    }
}

void led_feedback_enter_menu(LEDFeedbackController *ctrl, uint8_t page) {
    if (!ctrl) return;

    ctrl->in_menu = true;
    ctrl->current_page = page;
    ctrl->last_setting_value = 0xFF;  // Reset to force animation set on first update

    // Set LED X: page color with animation based on page type
    // 0 = blink (first page), 1 = glow (second page)
    NeopixelColor page_color = led_feedback_get_page_color(page);
    if (page < PAGE_COUNT && page_anim_type[page]) {
        // Second page of group: glow
        led_animation_set(&ctrl->mode_anim, ANIM_GLOW, page_color, ANIM_GLOW_PERIOD_MS);
    } else {
        // First page of group: blink
        led_animation_set(&ctrl->mode_anim, ANIM_BLINK, page_color, ANIM_BLINK_PERIOD_MS);
    }

    // Activity LED will show value feedback (set in update)
}

void led_feedback_exit_menu(LEDFeedbackController *ctrl) {
    if (!ctrl) return;

    ctrl->in_menu = false;

    // Restore static mode color
    NeopixelColor mode_color = led_feedback_get_mode_color(ctrl->current_mode);
    led_animation_set_static(&ctrl->mode_anim, mode_color);
}

void led_feedback_set_page(LEDFeedbackController *ctrl, uint8_t page) {
    if (!ctrl) return;
    if (page >= PAGE_COUNT) page = 0;

    ctrl->current_page = page;
    ctrl->last_setting_value = 0xFF;  // Reset to force animation set on next update

    if (ctrl->in_menu) {
        // Set LED X: page color with animation based on page type
        // 0 = blink (first page), 1 = glow (second page)
        NeopixelColor page_color = led_feedback_get_page_color(page);
        if (page_anim_type[page]) {
            // Second page of group: glow
            led_animation_set(&ctrl->mode_anim, ANIM_GLOW, page_color, ANIM_GLOW_PERIOD_MS);
        } else {
            // First page of group: blink
            led_animation_set(&ctrl->mode_anim, ANIM_BLINK, page_color, ANIM_BLINK_PERIOD_MS);
        }
    }
}

void led_feedback_flash(LEDFeedbackController *ctrl,
                        uint8_t r, uint8_t g, uint8_t b) {
    if (!ctrl) return;

    // Quick blink on activity LED
    NeopixelColor color = {r, g, b};
    led_animation_set(&ctrl->activity_anim, ANIM_BLINK, color, 200);
}

NeopixelColor led_feedback_get_mode_color(uint8_t mode) {
    if (mode >= MODE_COUNT) {
        return (NeopixelColor){0, 0, 0};
    }
    return mode_colors[mode];
}

NeopixelColor led_feedback_get_page_color(uint8_t page) {
    if (page >= PAGE_COUNT) {
        return (NeopixelColor){128, 128, 128};  // Gray for unknown
    }
    uint8_t mode = page_mode_map[page];
    if (mode >= MODE_COUNT) {
        return global_color;  // Global pages use white
    }
    return mode_colors[mode];
}
