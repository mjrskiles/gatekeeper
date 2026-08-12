#ifndef GK_HARDWARE_HAL_H
#define GK_HARDWARE_HAL_H

#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "hardware/hal_interface.h"

// ATtiny85 has PB0-PB5 (PB5 is typically RESET unless fuse is changed)
#define HAL_MAX_PIN 5

// Rev2 Pin Assignments (ATtiny85)
// See docs/planning/decision-records/001-rev2-architecture.md
#define BUTTON_A_PIN PB2                // Primary button (menu/mode)
#define BUTTON_B_PIN PB4                // Secondary button (value/action)
#define SIG_OUT_PIN PB1                 // CV output (directly to buffer circuit)
#define NEOPIXEL_PIN PB0                // WS2812B data line (future)
#define CV_IN_PIN PB3                   // CV input (future)

// Note: Neopixels are on PB0 but controlled via neopixel.c driver, not GPIO
// Note: Output LED is driven by the output buffer circuit from SIG_OUT_PIN

void hal_init(void);

void hal_set_pin(uint8_t pin);
void hal_clear_pin(uint8_t pin);
void hal_toggle_pin(uint8_t pin);

uint8_t hal_read_pin(uint8_t pin);

void hal_init_timer0(void);
uint32_t hal_millis(void);
void hal_delay_ms(uint32_t ms);
void hal_advance_time(uint32_t ms);
void hal_reset_time(void);

// EEPROM functions
uint8_t hal_eeprom_read_byte(uint16_t addr);
void hal_eeprom_write_byte(uint16_t addr, uint8_t value);
uint16_t hal_eeprom_read_word(uint16_t addr);
void hal_eeprom_write_word(uint16_t addr, uint16_t value);

// ADC functions (for analog CV input per ADR-004)
void hal_init_adc(void);
uint8_t hal_adc_read(uint8_t channel);

// Watchdog functions
void hal_wdt_enable(void);
void hal_wdt_reset(void);
void hal_wdt_disable(void);

#endif /* GK_HARDWARE_HAL_H */


