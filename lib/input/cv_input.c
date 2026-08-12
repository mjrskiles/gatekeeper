#include "input/cv_input.h"

/**
 * @file cv_input.c
 * @brief CV input processing with software hysteresis (per ADR-004)
 */

void cv_input_init(CVInput *cv) {
    cv_input_init_custom(cv, CV_DEFAULT_HIGH_THRESHOLD, CV_DEFAULT_LOW_THRESHOLD);
}

void cv_input_init_custom(CVInput *cv, uint8_t high_threshold, uint8_t low_threshold) {
    if (!cv) return;

    cv->high_threshold = high_threshold;
    cv->low_threshold = low_threshold;
    cv->last_adc_value = 0;
    cv->current_state = false;
}

bool cv_input_update(CVInput *cv, uint8_t adc_value) {
    if (!cv) return false;

    cv->last_adc_value = adc_value;

    if (cv->current_state) {
        // Currently HIGH - need to drop below low_threshold to go LOW
        if (adc_value < cv->low_threshold) {
            cv->current_state = false;
        }
    } else {
        // Currently LOW - need to rise above high_threshold to go HIGH
        if (adc_value > cv->high_threshold) {
            cv->current_state = true;
        }
    }

    return cv->current_state;
}

bool cv_input_get_state(const CVInput *cv) {
    if (!cv) return false;
    return cv->current_state;
}

uint8_t cv_input_get_adc_value(const CVInput *cv) {
    if (!cv) return 0;
    return cv->last_adc_value;
}

uint16_t cv_adc_to_millivolts(uint8_t adc_value) {
    // 255 = 5000mV, so mV = adc_value * 5000 / 255
    // Use 32-bit intermediate to avoid overflow
    return (uint16_t)(((uint32_t)adc_value * 5000) / 255);
}
