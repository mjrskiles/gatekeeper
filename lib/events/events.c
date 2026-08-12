#include "events/events.h"

void event_processor_init(EventProcessor *ep) {
    if (!ep) return;

    ep->status = 0;
    ep->ext_status = 0;
    ep->a_press_time = 0;
    ep->b_press_time = 0;
}

void event_processor_reset(EventProcessor *ep) {
    if (!ep) return;

    ep->status = 0;
    ep->ext_status = 0;
    ep->a_press_time = 0;
    ep->b_press_time = 0;
}

Event event_processor_update(EventProcessor *ep, const EventInput *input) {
    if (!ep || !input) return EVT_NONE;

    Event event = EVT_NONE;
    uint32_t now = input->current_time;

    // Update current input states
    STATUS_PUT(ep->status, EP_A_PRESSED, input->button_a);
    STATUS_PUT(ep->status, EP_B_PRESSED, input->button_b);
    STATUS_PUT(ep->status, EP_CV_STATE, input->cv_in);

    // === Button A edge detection ===
    bool a_pressed = STATUS_ANY(ep->status, EP_A_PRESSED);
    bool a_was_pressed = STATUS_ANY(ep->status, EP_A_LAST);

    // Rising edge (press)
    if (a_pressed && !a_was_pressed) {
        ep->a_press_time = now;
        STATUS_CLR(ep->status, EP_A_HOLD);
        ep->ext_status &= ~EP_B_TOUCHED_DURING_A;  // Clear B-touched flag on new A press
        event = EVT_A_PRESS;
    }
    // Falling edge (release)
    else if (!a_pressed && a_was_pressed) {
        // Check if it was a tap (short press) or longer hold
        if (!STATUS_ANY(ep->status, EP_A_HOLD)) {
            // Was short press - it's a tap
            event = EVT_A_TAP;
        } else {
            // Was a hold - check if it was a solo hold (no B touched, no compound fired)
            if (!(ep->ext_status & EP_B_TOUCHED_DURING_A) &&
                !(ep->ext_status & EP_COMPOUND_FIRED)) {
                // Solo A:hold → release = mode change (perform) or menu exit (menu)
                event = EVT_MODE_NEXT;
            } else {
                event = EVT_A_RELEASE;
            }
        }
        STATUS_CLR(ep->status, EP_A_HOLD);
    }
    // Hold detection (while still pressed)
    // Only emit EVT_A_HOLD for solo holds (B not pressed)
    // This allows menu exit on immediate hold without waiting for release
    else if (a_pressed && !STATUS_ANY(ep->status, EP_A_HOLD)) {
        if (now - ep->a_press_time >= EP_HOLD_THRESHOLD_MS) {
            STATUS_SET(ep->status, EP_A_HOLD);
            // Only emit hold event if B is not pressed (solo hold)
            if (!STATUS_ANY(ep->status, EP_B_PRESSED)) {
                event = EVT_A_HOLD;
            }
        }
    }

    // === Button B edge detection ===
    bool b_pressed = STATUS_ANY(ep->status, EP_B_PRESSED);
    bool b_was_pressed = STATUS_ANY(ep->status, EP_B_LAST);

    // Rising edge (press)
    if (b_pressed && !b_was_pressed) {
        ep->b_press_time = now;
        STATUS_CLR(ep->status, EP_B_HOLD);
        // Track B press during A hold (cancels solo A:hold gesture)
        if (STATUS_ANY(ep->status, EP_A_HOLD)) {
            ep->ext_status |= EP_B_TOUCHED_DURING_A;
        }
        // Only set event if we don't already have one from A
        if (event == EVT_NONE) {
            event = EVT_B_PRESS;
        }
    }
    // Falling edge (release)
    else if (!b_pressed && b_was_pressed) {
        if (event == EVT_NONE) {
            // Check if it was a tap or longer
            if (!STATUS_ANY(ep->status, EP_B_HOLD)) {
                event = EVT_B_TAP;
            } else {
                event = EVT_B_RELEASE;
            }
        }
        STATUS_CLR(ep->status, EP_B_HOLD);
    }
    // Hold detection (while still pressed)
    else if (b_pressed && !STATUS_ANY(ep->status, EP_B_HOLD)) {
        if (now - ep->b_press_time >= EP_HOLD_THRESHOLD_MS) {
            STATUS_SET(ep->status, EP_B_HOLD);
            if (event == EVT_NONE) {
                event = EVT_B_HOLD;
            }
        }
    }

    // === Compound gesture detection ===
    // Menu Enter: A held first, then B reaches hold threshold
    // (EVT_MODE_NEXT is now generated on A:hold release without B touch)
    //
    // Only fire once per gesture (cleared when both buttons released)
    if (!(ep->ext_status & EP_COMPOUND_FIRED)) {
        // B just reached hold while A is pressed, and A was pressed first
        if (event == EVT_B_HOLD &&
            STATUS_ANY(ep->status, EP_A_PRESSED) &&
            ep->a_press_time < ep->b_press_time) {
            event = EVT_MENU_TOGGLE;
            ep->ext_status |= EP_COMPOUND_FIRED;
        }
    }

    // Clear compound fired flag when both buttons are released
    if (!a_pressed && !b_pressed) {
        ep->ext_status &= ~EP_COMPOUND_FIRED;
    }

    // === CV edge detection ===
    bool cv_high = STATUS_ANY(ep->status, EP_CV_STATE);
    bool cv_was_high = STATUS_ANY(ep->status, EP_CV_LAST);

    if (event == EVT_NONE) {
        if (cv_high && !cv_was_high) {
            event = EVT_CV_RISE;
        } else if (!cv_high && cv_was_high) {
            event = EVT_CV_FALL;
        }
    }

    // === Update last states for next cycle ===
    STATUS_PUT(ep->status, EP_A_LAST, a_pressed);
    STATUS_PUT(ep->status, EP_B_LAST, b_pressed);
    STATUS_PUT(ep->status, EP_CV_LAST, cv_high);

    return event;
}

bool event_processor_a_pressed(const EventProcessor *ep) {
    if (!ep) return false;
    return STATUS_ANY(ep->status, EP_A_PRESSED);
}

bool event_processor_b_pressed(const EventProcessor *ep) {
    if (!ep) return false;
    return STATUS_ANY(ep->status, EP_B_PRESSED);
}

bool event_processor_a_holding(const EventProcessor *ep) {
    if (!ep) return false;
    return STATUS_ANY(ep->status, EP_A_HOLD);
}

bool event_processor_b_holding(const EventProcessor *ep) {
    if (!ep) return false;
    return STATUS_ANY(ep->status, EP_B_HOLD);
}
