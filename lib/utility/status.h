#ifndef GK_UTILITY_STATUS_H
#define GK_UTILITY_STATUS_H

/**
 * @file status.h
 * @brief Common macros for status word / bitmask manipulation
 *
 * Provides a consistent API for working with uint8_t status words
 * across all Gatekeeper modules. Using macros instead of inline
 * functions avoids call overhead on AVR.
 *
 * Convention:
 * - Status words are uint8_t fields named `status` or `flags`
 * - Bit definitions use MODULE_FLAGNAME pattern (e.g., BTN_PRESSED)
 * - Reserved bits should be documented and left as 0
 *
 * See ADR-002 and FDP-006 for design rationale.
 */

/**
 * Set bit(s) in status word.
 *
 * @param status  The status word (lvalue)
 * @param mask    Bit mask to set
 *
 * Example: STATUS_SET(btn->status, BTN_PRESSED | BTN_RISE);
 */
#define STATUS_SET(status, mask)    ((status) |= (mask))

/**
 * Clear bit(s) in status word.
 *
 * @param status  The status word (lvalue)
 * @param mask    Bit mask to clear
 *
 * Example: STATUS_CLR(btn->status, BTN_RISE | BTN_FALL);
 */
#define STATUS_CLR(status, mask)    ((status) &= ~(mask))

/**
 * Toggle bit(s) in status word.
 *
 * @param status  The status word (lvalue)
 * @param mask    Bit mask to toggle
 *
 * Example: STATUS_TGL(led->flags, LED_ON);
 */
#define STATUS_TGL(status, mask)    ((status) ^= (mask))

/**
 * Check if any bit(s) in mask are set.
 *
 * @param status  The status word
 * @param mask    Bit mask to check
 * @return        Non-zero if any bits in mask are set
 *
 * Example: if (STATUS_ANY(btn->status, BTN_RISE | BTN_FALL)) { ... }
 */
#define STATUS_ANY(status, mask)    (((status) & (mask)) != 0)

/**
 * Check if all bit(s) in mask are set.
 *
 * @param status  The status word
 * @param mask    Bit mask to check
 * @return        Non-zero if all bits in mask are set
 *
 * Example: if (STATUS_ALL(ep->status, EVT_A_PRESSED | EVT_A_HOLD)) { ... }
 */
#define STATUS_ALL(status, mask)    (((status) & (mask)) == (mask))

/**
 * Check if all bit(s) in mask are clear.
 *
 * @param status  The status word
 * @param mask    Bit mask to check
 * @return        Non-zero if all bits in mask are clear
 *
 * Example: if (STATUS_NONE(btn->status, BTN_PRESSED)) { ... }
 */
#define STATUS_NONE(status, mask)   (((status) & (mask)) == 0)

/**
 * Set or clear bit(s) based on boolean value.
 *
 * @param status  The status word (lvalue)
 * @param mask    Bit mask to modify
 * @param val     Boolean value (0 = clear, non-zero = set)
 *
 * Example: STATUS_PUT(btn->status, BTN_RAW, p_hal->read_pin(pin));
 */
#define STATUS_PUT(status, mask, val) \
    do { \
        if (val) { \
            STATUS_SET(status, mask); \
        } else { \
            STATUS_CLR(status, mask); \
        } \
    } while (0)

/**
 * Get the boolean value of a single bit.
 *
 * @param status  The status word
 * @param mask    Single bit mask
 * @return        0 or 1
 *
 * Example: bool pressed = STATUS_GET(btn->status, BTN_PRESSED);
 */
#define STATUS_GET(status, mask)    (STATUS_ANY(status, mask) ? 1 : 0)

#endif /* GK_UTILITY_STATUS_H */
