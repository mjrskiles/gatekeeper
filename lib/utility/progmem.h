#ifndef GK_UTILITY_PROGMEM_H
#define GK_UTILITY_PROGMEM_H

/**
 * @file progmem.h
 * @brief PROGMEM abstraction for cross-platform compatibility
 *
 * On AVR targets, PROGMEM places const data in flash memory instead of RAM.
 * This is critical for ATtiny85 with only 512 bytes of RAM.
 *
 * On x86 (tests/simulator), PROGMEM is not available, so we provide
 * no-op macros that allow the same code to compile and run.
 *
 * Usage:
 *   // Declaration
 *   static const State my_states[] PROGMEM_ATTR = { ... };
 *
 *   // Reading a byte
 *   uint8_t val = PROGMEM_READ_BYTE(&my_states[i].id);
 *
 *   // Reading a word (16-bit)
 *   uint16_t val = PROGMEM_READ_WORD(&my_data[i]);
 *
 *   // Reading a pointer
 *   void (*func)(void) = PROGMEM_READ_PTR(&my_states[i].on_enter);
 *
 *   // Copying a struct from PROGMEM to RAM
 *   State s;
 *   PROGMEM_READ_STRUCT(&s, &my_states[i], sizeof(State));
 */

#if defined(__AVR__)
    #include <avr/pgmspace.h>

    #define PROGMEM_ATTR            PROGMEM
    #define PROGMEM_READ_BYTE(addr) pgm_read_byte(addr)
    #define PROGMEM_READ_WORD(addr) pgm_read_word(addr)
    #define PROGMEM_READ_PTR(addr)  ((void*)pgm_read_ptr(addr))
    #define PROGMEM_READ_STRUCT(dest, src, size) memcpy_P(dest, src, size)

#else
    /* x86 / test / simulator builds - no PROGMEM needed */
    #include <string.h>

    #define PROGMEM_ATTR            /* nothing */
    #define PROGMEM_READ_BYTE(addr) (*(const uint8_t*)(addr))
    #define PROGMEM_READ_WORD(addr) (*(const uint16_t*)(addr))
    #define PROGMEM_READ_PTR(addr)  (*(void* const*)(addr))
    #define PROGMEM_READ_STRUCT(dest, src, size) memcpy(dest, src, size)

#endif

#endif /* GK_UTILITY_PROGMEM_H */
