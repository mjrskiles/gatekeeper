#ifndef GK_OUTPUT_NEOPIXEL_H
#define GK_OUTPUT_NEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file neopixel.h
 * @brief WS2812B (Neopixel) driver interface
 *
 * Provides a platform-independent interface for controlling WS2812B LEDs.
 * The actual implementation is platform-specific:
 * - AVR: Bit-banged with cycle-accurate inline assembly
 * - Test/Sim: Mock implementation that stores values for inspection
 *
 * Gatekeeper Rev2 has 2 Neopixels in series on PB0:
 * - LED 0 (LED_MODE): Mode/page indicator
 * - LED 1 (LED_ACTIVITY): Output state/activity indicator
 *
 * See AP-004 for implementation details.
 */

#define NEOPIXEL_COUNT  2
#define LED_MODE        0   // First LED in chain (mode indicator)
#define LED_ACTIVITY    1   // Second LED in chain (activity indicator)

/**
 * RGB color structure
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} NeopixelColor;

/**
 * Initialize the neopixel driver.
 *
 * Configures the data pin as output and clears all LEDs.
 */
void neopixel_init(void);

/**
 * Set LED color using Color struct.
 *
 * @param index LED index (0 or 1)
 * @param color RGB color
 */
void neopixel_set_color(uint8_t index, NeopixelColor color);

/**
 * Set LED color using individual RGB values.
 *
 * @param index LED index (0 or 1)
 * @param r     Red component (0-255)
 * @param g     Green component (0-255)
 * @param b     Blue component (0-255)
 */
void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Get current color of an LED.
 *
 * Returns the buffered color (may not yet be transmitted).
 *
 * @param index LED index (0 or 1)
 * @return      Current color
 */
NeopixelColor neopixel_get_color(uint8_t index);

/**
 * Clear all LEDs (set to black).
 */
void neopixel_clear(void);

/**
 * Transmit buffered colors to LEDs.
 *
 * Only transmits if buffer has changed since last flush.
 * On AVR, this disables interrupts briefly (~60µs for 2 LEDs).
 */
void neopixel_flush(void);

/**
 * Check if buffer needs flushing.
 *
 * @return true if colors have changed since last flush
 */
bool neopixel_is_dirty(void);

#endif /* GK_OUTPUT_NEOPIXEL_H */
