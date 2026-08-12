#include "output/cv_output.h"

void cv_output_init(CVOutput *cv_output, uint8_t pin) {
    if (!cv_output) return;

    cv_output->pin = pin;
    cv_output->status = 0;
    cv_output->pulse_start = 0;
}

void cv_output_reset(CVOutput *cv_output) {
    if (!cv_output) return;

    cv_output->status = 0;
    cv_output->pulse_start = 0;
    p_hal->clear_pin(cv_output->pin);
}

void cv_output_set(CVOutput *cv_output) {
    STATUS_SET(cv_output->status, CVOUT_STATE);
    p_hal->set_pin(cv_output->pin);
}

void cv_output_clear(CVOutput *cv_output) {
    STATUS_CLR(cv_output->status, CVOUT_STATE);
    p_hal->clear_pin(cv_output->pin);
}

bool cv_output_update_gate(CVOutput *cv_output, bool input_state) {
    if (input_state) {
        cv_output_set(cv_output);
    } else {
        cv_output_clear(cv_output);
    }
    return STATUS_ANY(cv_output->status, CVOUT_STATE);
}

bool cv_output_update_pulse(CVOutput *cv_output, bool input_state) {
    bool last_input = STATUS_ANY(cv_output->status, CVOUT_LAST_IN);

    // Rising edge triggers new pulse
    if (input_state && !last_input) {
        cv_output->pulse_start = p_hal->millis();
        STATUS_SET(cv_output->status, CVOUT_PULSE);
        cv_output_set(cv_output);
    }

    // Check pulse expiry
    if (STATUS_ANY(cv_output->status, CVOUT_PULSE)) {
        uint32_t current_time = p_hal->millis();
        if (current_time - cv_output->pulse_start >= PULSE_DURATION_MS) {
            STATUS_CLR(cv_output->status, CVOUT_PULSE);
            cv_output_clear(cv_output);
        }
    }

    STATUS_PUT(cv_output->status, CVOUT_LAST_IN, input_state);
    return STATUS_ANY(cv_output->status, CVOUT_STATE);
}

bool cv_output_update_toggle(CVOutput *cv_output, bool input_state) {
    bool last_input = STATUS_ANY(cv_output->status, CVOUT_LAST_IN);

    // Only toggle on rising edge (false -> true transition)
    if (input_state && !last_input) {
        if (STATUS_ANY(cv_output->status, CVOUT_STATE)) {
            cv_output_clear(cv_output);
        } else {
            cv_output_set(cv_output);
        }
    }

    STATUS_PUT(cv_output->status, CVOUT_LAST_IN, input_state);
    return STATUS_ANY(cv_output->status, CVOUT_STATE);
}
