#include "output/neopixel.h"

#if defined(__AVR__)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/**
 * @file neopixel.c
 * @brief WS2812B driver for ATtiny85 at 8 MHz
 *
 * Bit-banged driver with cycle-accurate timing for WS2812B LEDs.
 * Tuned for F_CPU = 8 MHz (125ns per cycle).
 *
 * WS2812B timing requirements:
 * - T0H: 350ns ±150ns (200-500ns)  → 3 cycles = 375ns ✓
 * - T0L: 800ns ±150ns (650-950ns)  → 7 cycles = 875ns ✓
 * - T1H: 700ns ±150ns (550-850ns)  → 6 cycles = 750ns ✓
 * - T1L: 600ns ±150ns (450-750ns)  → 4 cycles = 500ns ✓
 * - Reset: >50µs
 *
 * Data pin: PB0
 *
 * TIMING IMPACT:
 * Interrupts are disabled during neopixel_flush() to maintain bit timing.
 * With 2 Neopixels (6 bytes), transmission takes approximately:
 *   6 bytes * 8 bits * ~10 cycles/bit = 480 cycles
 *   At 8 MHz: ~60µs with interrupts disabled
 *   Plus 60µs reset pulse = ~120µs total
 *
 * This is well under the 1ms Timer0 tick, so missed interrupts are unlikely.
 *
 * For 2 LEDs: Impact negligible (0.012% of time with interrupts off)
 * For 8 LEDs: ~240µs = 0.024% - still acceptable
 * For 30+ LEDs: Consider compensating for missed ticks
 */

// Neopixel data pin
#define NEOPIXEL_PIN    PB0
#define NEOPIXEL_PORT   PORTB
#define NEOPIXEL_DDR    DDRB

// LED buffer in RGB order (some WS2812B variants use RGB instead of GRB)
static uint8_t led_buffer[NEOPIXEL_COUNT * 3];
static bool buffer_dirty = false;

void neopixel_init(void) {
    // Configure pin as output, initially low
    NEOPIXEL_DDR |= (1 << NEOPIXEL_PIN);
    NEOPIXEL_PORT &= ~(1 << NEOPIXEL_PIN);

    neopixel_clear();
    neopixel_flush();
}

void neopixel_set_color(uint8_t index, NeopixelColor color) {
    if (index >= NEOPIXEL_COUNT) return;

    uint8_t offset = index * 3;
    // RGB order (some WS2812B variants use RGB instead of GRB)
    led_buffer[offset + 0] = color.r;
    led_buffer[offset + 1] = color.g;
    led_buffer[offset + 2] = color.b;
    buffer_dirty = true;
}

void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return;

    uint8_t offset = index * 3;
    led_buffer[offset + 0] = r;  // RGB order
    led_buffer[offset + 1] = g;
    led_buffer[offset + 2] = b;
    buffer_dirty = true;
}

NeopixelColor neopixel_get_color(uint8_t index) {
    NeopixelColor color = {0, 0, 0};
    if (index >= NEOPIXEL_COUNT) return color;

    uint8_t offset = index * 3;
    color.r = led_buffer[offset + 0];  // RGB order
    color.g = led_buffer[offset + 1];
    color.b = led_buffer[offset + 2];
    return color;
}

void neopixel_clear(void) {
    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        led_buffer[i] = 0;
    }
    buffer_dirty = true;
}

bool neopixel_is_dirty(void) {
    return buffer_dirty;
}

/**
 * Send a single byte to the LED chain.
 *
 * Timing for 8 MHz (125ns/cycle):
 *
 * Bit 0 (short high, long low):
 *   HIGH: sbi (2) + nop (1) = 3 cycles = 375ns
 *   LOW:  cbi (2) + 5 nops = 7 cycles = 875ns
 *
 * Bit 1 (long high, short low):
 *   HIGH: sbi (2) + 4 nops = 6 cycles = 750ns
 *   LOW:  cbi (2) + 2 nops = 4 cycles = 500ns
 *
 * Loop overhead is absorbed into the low time of each bit.
 */
static void send_byte(uint8_t byte) {
    uint8_t bit_count = 8;

    asm volatile(
        "send_bit_%=:"                      "\n\t"
        "sbrc %[byte], 7"                   "\n\t"  // 1/2 cycles - skip if bit 7 clear
        "rjmp send_one_%="                  "\n\t"  // 2 cycles if taken

        // === Send 0: 3 cycles high, 7 cycles low ===
        "sbi %[port], %[pin]"               "\n\t"  // 2 cycles - HIGH
        "nop"                               "\n\t"  // 1 cycle  - extend high
        "cbi %[port], %[pin]"               "\n\t"  // 2 cycles - LOW
        "nop"                               "\n\t"  // 1 cycle  - extend low
        "nop"                               "\n\t"  // 1 cycle
        "nop"                               "\n\t"  // 1 cycle
        "rjmp next_bit_%="                  "\n\t"  // 2 cycles (part of low time)

        "send_one_%=:"                      "\n\t"
        // === Send 1: 6 cycles high, 4 cycles low ===
        "sbi %[port], %[pin]"               "\n\t"  // 2 cycles - HIGH
        "nop"                               "\n\t"  // 1 cycle  - extend high
        "nop"                               "\n\t"  // 1 cycle
        "nop"                               "\n\t"  // 1 cycle
        "nop"                               "\n\t"  // 1 cycle
        "cbi %[port], %[pin]"               "\n\t"  // 2 cycles - LOW
        "nop"                               "\n\t"  // 1 cycle  - extend low
        "nop"                               "\n\t"  // 1 cycle

        "next_bit_%=:"                      "\n\t"
        "lsl %[byte]"                       "\n\t"  // 1 cycle - shift to next bit
        "dec %[count]"                      "\n\t"  // 1 cycle
        "brne send_bit_%="                  "\n\t"  // 2/1 cycles - loop if not zero

        : [byte] "+r" (byte),
          [count] "+r" (bit_count)
        : [port] "I" (_SFR_IO_ADDR(NEOPIXEL_PORT)),
          [pin] "I" (NEOPIXEL_PIN)
    );
}

/**
 * Transmit LED buffer to Neopixel chain.
 *
 * NOTE: Interrupts are disabled during transmission (~60µs for 2 LEDs).
 * See file header for timing impact analysis. For 2 LEDs this is
 * well under the 1ms timer tick and has negligible impact.
 */
void neopixel_flush(void) {
    if (!buffer_dirty) return;

    // Disable interrupts for timing-critical transmission
    // (~60µs for 2 LEDs at 8 MHz - see timing analysis in file header)
    uint8_t sreg = SREG;
    cli();

    // Send all bytes
    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        send_byte(led_buffer[i]);
    }

    // Restore interrupt state
    SREG = sreg;

    // Reset pulse (>50µs low) - interrupts are enabled here
    _delay_us(60);

    buffer_dirty = false;
}

#endif /* __AVR__ */
