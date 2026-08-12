#include "output/led_animation.h"
#include "output/neopixel.h"

/**
 * @file led_animation.c
 * @brief LED animation engine implementation
 */

void led_animation_init(LEDAnimation *anim) {
    if (!anim) return;

    anim->type = ANIM_NONE;
    anim->base_color = (NeopixelColor){0, 0, 0};
    anim->period_ms = ANIM_BLINK_PERIOD_MS;
    anim->last_update = 0;
    anim->phase = 0;
    anim->current_on = true;
}

void led_animation_set(LEDAnimation *anim, AnimationType type,
                       NeopixelColor color, uint16_t period_ms) {
    if (!anim) return;

    anim->type = type;
    anim->base_color = color;
    anim->period_ms = period_ms > 0 ? period_ms : ANIM_BLINK_PERIOD_MS;
    anim->phase = 0;
    anim->current_on = true;
}

void led_animation_set_static(LEDAnimation *anim, NeopixelColor color) {
    if (!anim) return;

    anim->type = ANIM_NONE;
    anim->base_color = color;
}

NeopixelColor led_color_scale(NeopixelColor color, uint8_t brightness) {
    NeopixelColor scaled;
    scaled.r = ((uint16_t)color.r * brightness) / 255;
    scaled.g = ((uint16_t)color.g * brightness) / 255;
    scaled.b = ((uint16_t)color.b * brightness) / 255;
    return scaled;
}

void led_animation_update(LEDAnimation *anim, uint8_t led_index,
                          uint32_t current_time) {
    if (!anim) return;

    switch (anim->type) {
        case ANIM_NONE:
            // Static color - just set it
            neopixel_set_color(led_index, anim->base_color);
            break;

        case ANIM_BLINK: {
            // Toggle on/off at half-period intervals
            uint16_t half_period = anim->period_ms / 2;
            if (current_time - anim->last_update >= half_period) {
                anim->last_update = current_time;
                anim->current_on = !anim->current_on;
            }

            if (anim->current_on) {
                neopixel_set_color(led_index, anim->base_color);
            } else {
                neopixel_set_rgb(led_index, 0, 0, 0);
            }
            break;
        }

        case ANIM_GLOW: {
            // Smooth triangle wave brightness
            // Calculate phase position (0-255) within period
            uint32_t phase_time = current_time % anim->period_ms;
            anim->phase = (uint8_t)((phase_time * 255) / anim->period_ms);

            // Triangle wave: ramp up 0-127, ramp down 128-255
            uint8_t brightness;
            if (anim->phase < 128) {
                brightness = anim->phase * 2;
            } else {
                brightness = (255 - anim->phase) * 2;
            }

            NeopixelColor scaled = led_color_scale(anim->base_color, brightness);
            neopixel_set_color(led_index, scaled);
            break;
        }
    }
}

void led_animation_stop(LEDAnimation *anim, uint8_t led_index) {
    if (!anim) return;

    anim->type = ANIM_NONE;
    anim->base_color = (NeopixelColor){0, 0, 0};
    neopixel_set_rgb(led_index, 0, 0, 0);
}
