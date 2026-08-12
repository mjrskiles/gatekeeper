#ifndef GK_OUTPUT_LED_ANIMATION_H
#define GK_OUTPUT_LED_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "output/neopixel.h"

/**
 * @file led_animation.h
 * @brief LED animation engine for blink and glow effects
 *
 * Provides time-based animations that can be applied to LEDs.
 * Animations are updated each main loop iteration.
 *
 * See AP-004 for implementation details.
 */

/**
 * Animation types
 */
typedef enum {
    ANIM_NONE = 0,      // Static color (no animation)
    ANIM_BLINK,         // On/off blinking
    ANIM_GLOW,          // Smooth brightness pulsing
} AnimationType;

/**
 * Animation state for a single LED
 */
typedef struct {
    AnimationType type;
    NeopixelColor base_color;   // Color to animate
    uint16_t period_ms;         // Full cycle period
    uint32_t last_update;       // For blink: last toggle time
    uint8_t phase;              // 0-255 position in cycle
    bool current_on;            // For blink: current state
} LEDAnimation;

/**
 * Default animation periods
 */
#define ANIM_BLINK_PERIOD_MS    500     // 2 Hz blink
#define ANIM_GLOW_PERIOD_MS     1000    // 1 Hz glow cycle

/**
 * Initialize an animation struct to defaults.
 *
 * @param anim  Animation struct to initialize
 */
void led_animation_init(LEDAnimation *anim);

/**
 * Configure an animation.
 *
 * @param anim      Animation struct
 * @param type      Animation type
 * @param color     Base color for animation
 * @param period_ms Animation period in milliseconds
 */
void led_animation_set(LEDAnimation *anim, AnimationType type,
                       NeopixelColor color, uint16_t period_ms);

/**
 * Set animation to static color (no animation).
 *
 * @param anim  Animation struct
 * @param color Static color to display
 */
void led_animation_set_static(LEDAnimation *anim, NeopixelColor color);

/**
 * Update animation and apply to LED.
 *
 * Call every main loop iteration. Calculates current animation
 * state and sets the LED color accordingly.
 *
 * @param anim          Animation struct
 * @param led_index     LED to update (LED_MODE or LED_ACTIVITY)
 * @param current_time  Current time in milliseconds
 */
void led_animation_update(LEDAnimation *anim, uint8_t led_index,
                          uint32_t current_time);

/**
 * Stop animation and turn off LED.
 *
 * @param anim      Animation struct
 * @param led_index LED to turn off
 */
void led_animation_stop(LEDAnimation *anim, uint8_t led_index);

/**
 * Scale a color by brightness (0-255).
 *
 * @param color      Base color
 * @param brightness Brightness level (0=off, 255=full)
 * @return           Scaled color
 */
NeopixelColor led_color_scale(NeopixelColor color, uint8_t brightness);

#endif /* GK_OUTPUT_LED_ANIMATION_H */
