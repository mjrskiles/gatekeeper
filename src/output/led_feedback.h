#ifndef GK_OUTPUT_LED_FEEDBACK_H
#define GK_OUTPUT_LED_FEEDBACK_H

#include <stdint.h>
#include <stdbool.h>
#include "modes/mode_handlers.h"
#include "output/neopixel.h"
#include "output/led_animation.h"

/**
 * @file led_feedback.h
 * @brief High-level LED feedback controller
 *
 * Provides the bridge between mode handlers (which produce LEDFeedback)
 * and the neopixel driver (which displays colors). Handles animations
 * and menu-specific displays.
 *
 * LED layout:
 * - LED_MODE (0): Shows mode color in Perform, page color in Menu
 * - LED_ACTIVITY (1): Shows output state/activity
 *
 * See AP-004 for implementation details.
 */

/**
 * LED feedback controller state
 */
typedef struct {
    LEDAnimation mode_anim;         // Animation for mode LED
    LEDAnimation activity_anim;     // Animation for activity LED
    bool in_menu;                   // Currently in menu mode
    uint8_t current_mode;           // Current mode (for color lookup)
    uint8_t current_page;           // Current page (when in menu)
    uint8_t last_setting_value;     // Previous menu setting value (for change detection)
} LEDFeedbackController;

/**
 * Initialize the LED feedback controller.
 *
 * Initializes neopixel driver and clears LEDs.
 *
 * @param ctrl  Controller struct to initialize
 */
void led_feedback_init(LEDFeedbackController *ctrl);

/**
 * Update LED feedback from mode handler output.
 *
 * Call every main loop iteration. Updates animations and
 * flushes changes to LEDs.
 *
 * @param ctrl          Controller struct
 * @param feedback      LED feedback from mode handler
 * @param current_time  Current time in milliseconds
 */
void led_feedback_update(LEDFeedbackController *ctrl,
                         const LEDFeedback *feedback,
                         uint32_t current_time);

/**
 * Set mode for LED display (updates mode LED color).
 *
 * @param ctrl  Controller struct
 * @param mode  Mode index (MODE_GATE, MODE_TRIGGER, etc.)
 */
void led_feedback_set_mode(LEDFeedbackController *ctrl, uint8_t mode);

/**
 * Enter menu mode (changes LED behavior).
 *
 * @param ctrl  Controller struct
 * @param page  Initial menu page
 */
void led_feedback_enter_menu(LEDFeedbackController *ctrl, uint8_t page);

/**
 * Exit menu mode (return to perform mode display).
 *
 * @param ctrl  Controller struct
 */
void led_feedback_exit_menu(LEDFeedbackController *ctrl);

/**
 * Set current menu page (updates mode LED color).
 *
 * @param ctrl  Controller struct
 * @param page  Page index
 */
void led_feedback_set_page(LEDFeedbackController *ctrl, uint8_t page);

/**
 * Show a brief flash on activity LED (for user feedback).
 *
 * @param ctrl  Controller struct
 * @param r     Red component
 * @param g     Green component
 * @param b     Blue component
 */
void led_feedback_flash(LEDFeedbackController *ctrl,
                        uint8_t r, uint8_t g, uint8_t b);

/**
 * Get mode color for a given mode index.
 *
 * @param mode  Mode index
 * @return      RGB color for that mode
 */
NeopixelColor led_feedback_get_mode_color(uint8_t mode);

/**
 * Get page color for a given menu page.
 *
 * @param page  Page index
 * @return      RGB color for that page
 */
NeopixelColor led_feedback_get_page_color(uint8_t page);

#endif /* GK_OUTPUT_LED_FEEDBACK_H */
