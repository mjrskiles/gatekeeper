#include "input/button.h"

bool button_init(Button *button, uint8_t pin) {
    if (!button) return false;
    if (pin > p_hal->max_pin) return false;

    button->pin = pin;
    button->status = 0;  // Clear all flags
    button->last_tap_time = 0;
    button_reset(button);

    return true;
}

void button_reset(Button *button) {
    if (!button) return;

    button->status = 0;  // Clear all flags
    button->tap_count = 0;
    button->last_rise_time = 0;
    button->last_fall_time = 0;
    button->last_tap_time = 0;
}

bool button_has_rising_edge(Button *button) {
    // Rising edge: raw is high, last state was low
    // Take into account debounce time.
    if (STATUS_ANY(button->status, BTN_RAW) &&
        STATUS_NONE(button->status, BTN_LAST)) {
        if (p_hal->millis() - button->last_rise_time > EDGE_DEBOUNCE_MS) {
            button->last_rise_time = p_hal->millis();
            return true;
        }
    }
    return false;
}

bool button_has_falling_edge(Button *button) {
    // Falling edge: raw is low, last state was high
    if (STATUS_NONE(button->status, BTN_RAW) &&
        STATUS_ANY(button->status, BTN_LAST)) {
        if (p_hal->millis() - button->last_fall_time > EDGE_DEBOUNCE_MS) {
            button->last_fall_time = p_hal->millis();
            return true;
        }
    }
    return false;
}

void button_update(Button *button) {
    // Read raw pin state (active-low: pressed = LOW, so invert)
    STATUS_PUT(button->status, BTN_RAW, !p_hal->read_pin(button->pin));

    // Clear edge flags at start of cycle
    STATUS_CLR(button->status, BTN_RISE | BTN_FALL);

    // Detect rising edge (button press)
    if (button_has_rising_edge(button)) {
        STATUS_SET(button->status, BTN_RISE | BTN_PRESSED);
    }

    // Detect falling edge (button release)
    if (button_has_falling_edge(button)) {
        STATUS_SET(button->status, BTN_FALL);
        STATUS_CLR(button->status, BTN_PRESSED);
    }

    // Check if we've requested a mode change
    if (button_detect_config_action(button)) {
        STATUS_SET(button->status, BTN_CONFIG);
    }

    // Update last state to match current pressed state
    STATUS_PUT(button->status, BTN_LAST, STATUS_ANY(button->status, BTN_PRESSED));
}

void button_consume_config_action(Button *button) {
    STATUS_CLR(button->status, BTN_CONFIG);
}

bool button_detect_config_action(Button *button) {
    uint32_t current_time = p_hal->millis();
    bool action_detected = false;

    // On rising edge (new press)
    if (STATUS_ANY(button->status, BTN_RISE)) {
        // Check if this tap is within the timeout window
        if (current_time - button->last_tap_time <= TAP_TIMEOUT_MS) {
            button->tap_count++;
        } else {
            // Too much time passed, reset tap count
            button->tap_count = 1;
        }
        button->last_tap_time = current_time;

        // If we've reached required taps, start counting hold time
        if (button->tap_count >= TAPS_TO_CHANGE) {
            STATUS_SET(button->status, BTN_COUNTING);
        }
    }

    // Check for hold on the Nth tap (hold the last tap to trigger)
    if (STATUS_ANY(button->status, BTN_COUNTING) &&
        STATUS_ANY(button->status, BTN_PRESSED)) {
        if (current_time - button->last_tap_time >= HOLD_TIME_MS) {
            action_detected = true;
            STATUS_CLR(button->status, BTN_COUNTING);
            button->tap_count = 0;
        }
    }

    // Reset if button released before hold completed
    if (STATUS_NONE(button->status, BTN_PRESSED)) {
        STATUS_CLR(button->status, BTN_COUNTING);
        if (current_time - button->last_tap_time > TAP_TIMEOUT_MS) {
            button->tap_count = 0;
        }
    }

    return action_detected;
}
