#include "command_handler.h"
#include "sim_hal.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file command_handler.c
 * @brief JSON command parser implementation using cJSON
 *
 * Parses JSON commands for controlling the simulator.
 */

// Command type names
static const char *cmd_type_names[] = {
    "unknown", "button", "cv_manual", "cv_lfo", "cv_envelope",
    "cv_gate", "cv_trigger", "cv_wavetable", "fault_adc", "fault_eeprom",
    "reset", "quit"
};

const char* command_type_str(CommandType type) {
    if (type >= sizeof(cmd_type_names) / sizeof(cmd_type_names[0])) {
        return "invalid";
    }
    return cmd_type_names[type];
}

// =============================================================================
// Command Handlers
// =============================================================================

static CommandResult handle_button(cJSON *json) {
    CommandResult result = { CMD_BUTTON, false, false, "" };

    cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");

    if (!cJSON_IsString(id) || !id->valuestring) {
        snprintf(result.error, sizeof(result.error), "missing 'id' field");
        return result;
    }
    if (!cJSON_IsBool(state)) {
        snprintf(result.error, sizeof(result.error), "missing 'state' field");
        return result;
    }

    if (strcmp(id->valuestring, "a") == 0) {
        sim_set_button_a(cJSON_IsTrue(state));
        result.success = true;
    } else if (strcmp(id->valuestring, "b") == 0) {
        sim_set_button_b(cJSON_IsTrue(state));
        result.success = true;
    } else {
        snprintf(result.error, sizeof(result.error), "invalid button id: %s", id->valuestring);
    }

    return result;
}

static CommandResult handle_cv_manual(cJSON *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_MANUAL, false, false, "" };

    cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
    if (!cJSON_IsNumber(value)) {
        snprintf(result.error, sizeof(result.error), "missing 'value' field");
        return result;
    }

    double v = value->valuedouble;
    if (v < 0) v = 0;
    if (v > 255) v = 255;

    cv_source_set_manual(cv_source, (uint8_t)v);
    result.success = true;
    return result;
}

static CommandResult handle_cv_lfo(cJSON *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_LFO, false, false, "" };

    // Get optional parameters with defaults
    double freq_hz = 1.0;
    double min_val = 0;
    double max_val = 255;
    const char *shape_str = "sine";

    cJSON *freq = cJSON_GetObjectItemCaseSensitive(json, "freq_hz");
    cJSON *min_j = cJSON_GetObjectItemCaseSensitive(json, "min");
    cJSON *max_j = cJSON_GetObjectItemCaseSensitive(json, "max");
    cJSON *shape = cJSON_GetObjectItemCaseSensitive(json, "shape");

    if (cJSON_IsNumber(freq)) freq_hz = freq->valuedouble;
    if (cJSON_IsNumber(min_j)) min_val = min_j->valuedouble;
    if (cJSON_IsNumber(max_j)) max_val = max_j->valuedouble;
    if (cJSON_IsString(shape) && shape->valuestring) shape_str = shape->valuestring;

    // Parse shape
    LFOShape lfo_shape = LFO_SINE;
    if (strcmp(shape_str, "sine") == 0) lfo_shape = LFO_SINE;
    else if (strcmp(shape_str, "tri") == 0 || strcmp(shape_str, "triangle") == 0) lfo_shape = LFO_TRI;
    else if (strcmp(shape_str, "saw") == 0 || strcmp(shape_str, "sawtooth") == 0) lfo_shape = LFO_SAW;
    else if (strcmp(shape_str, "square") == 0) lfo_shape = LFO_SQUARE;
    else if (strcmp(shape_str, "random") == 0 || strcmp(shape_str, "sh") == 0) lfo_shape = LFO_RANDOM;

    // Clamp values
    if (freq_hz < 0.01) freq_hz = 0.01;
    if (freq_hz > 100) freq_hz = 100;
    if (min_val < 0) min_val = 0;
    if (min_val > 255) min_val = 255;
    if (max_val < 0) max_val = 0;
    if (max_val > 255) max_val = 255;

    cv_source_set_lfo(cv_source, (float)freq_hz, lfo_shape,
                      (uint8_t)min_val, (uint8_t)max_val);
    result.success = true;
    return result;
}

static CommandResult handle_cv_envelope(cJSON *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_ENVELOPE, false, false, "" };

    // Get optional parameters with defaults
    double attack = 10;
    double decay = 100;
    double sustain = 200;
    double release = 200;

    cJSON *attack_j = cJSON_GetObjectItemCaseSensitive(json, "attack_ms");
    cJSON *decay_j = cJSON_GetObjectItemCaseSensitive(json, "decay_ms");
    cJSON *sustain_j = cJSON_GetObjectItemCaseSensitive(json, "sustain");
    cJSON *release_j = cJSON_GetObjectItemCaseSensitive(json, "release_ms");

    if (cJSON_IsNumber(attack_j)) attack = attack_j->valuedouble;
    if (cJSON_IsNumber(decay_j)) decay = decay_j->valuedouble;
    if (cJSON_IsNumber(sustain_j)) sustain = sustain_j->valuedouble;
    if (cJSON_IsNumber(release_j)) release = release_j->valuedouble;

    // Clamp values
    if (attack < 0) attack = 0;
    if (decay < 0) decay = 0;
    if (sustain < 0) sustain = 0;
    if (sustain > 255) sustain = 255;
    if (release < 0) release = 0;

    cv_source_set_envelope(cv_source,
                           (uint16_t)attack,
                           (uint16_t)decay,
                           (uint8_t)sustain,
                           (uint16_t)release);
    result.success = true;
    return result;
}

static CommandResult handle_cv_gate(cJSON *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_GATE, false, false, "" };

    cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");
    if (!cJSON_IsBool(state)) {
        snprintf(result.error, sizeof(result.error), "missing 'state' field");
        return result;
    }

    if (cJSON_IsTrue(state)) {
        cv_source_gate_on(cv_source);
    } else {
        cv_source_gate_off(cv_source);
    }
    result.success = true;
    return result;
}

static CommandResult handle_cv_trigger(CVSource *cv_source) {
    CommandResult result = { CMD_CV_TRIGGER, true, false, "" };
    cv_source_trigger(cv_source);
    return result;
}

static CommandResult handle_fault_adc(cJSON *json) {
    CommandResult result = { CMD_FAULT_ADC, false, false, "" };

    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (!cJSON_IsString(mode) || !mode->valuestring) {
        snprintf(result.error, sizeof(result.error), "missing 'mode' field");
        return result;
    }

    // Parse mode string
    if (strcmp(mode->valuestring, "normal") == 0) {
        sim_adc_set_mode(SIM_ADC_NORMAL);
        result.success = true;
    } else if (strcmp(mode->valuestring, "timeout") == 0) {
        sim_adc_set_mode(SIM_ADC_TIMEOUT);
        result.success = true;
    } else if (strcmp(mode->valuestring, "stuck_low") == 0) {
        sim_adc_set_mode(SIM_ADC_STUCK_LOW);
        result.success = true;
    } else if (strcmp(mode->valuestring, "stuck_high") == 0) {
        sim_adc_set_mode(SIM_ADC_STUCK_HIGH);
        result.success = true;
    } else if (strcmp(mode->valuestring, "noisy") == 0) {
        sim_adc_set_mode(SIM_ADC_NOISY);
        // Optional amplitude parameter
        cJSON *amplitude = cJSON_GetObjectItemCaseSensitive(json, "amplitude");
        if (cJSON_IsNumber(amplitude)) {
            int amp = (int)amplitude->valuedouble;
            if (amp < 0) amp = 0;
            if (amp > 127) amp = 127;
            sim_adc_set_noise_amplitude((uint8_t)amp);
        }
        result.success = true;
    } else {
        snprintf(result.error, sizeof(result.error), "invalid mode: %s", mode->valuestring);
    }

    return result;
}

static CommandResult handle_fault_eeprom(cJSON *json) {
    CommandResult result = { CMD_FAULT_EEPROM, false, false, "" };

    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (!cJSON_IsString(mode) || !mode->valuestring) {
        snprintf(result.error, sizeof(result.error), "missing 'mode' field");
        return result;
    }

    // Parse mode string
    if (strcmp(mode->valuestring, "normal") == 0) {
        sim_eeprom_set_mode(SIM_EEPROM_NORMAL);
        result.success = true;
    } else if (strcmp(mode->valuestring, "write_fail") == 0) {
        sim_eeprom_set_mode(SIM_EEPROM_WRITE_FAIL);
        result.success = true;
    } else if (strcmp(mode->valuestring, "read_ff") == 0) {
        sim_eeprom_set_mode(SIM_EEPROM_READ_FF);
        result.success = true;
    } else if (strcmp(mode->valuestring, "corrupt") == 0) {
        sim_eeprom_set_mode(SIM_EEPROM_CORRUPT);
        result.success = true;
    } else {
        snprintf(result.error, sizeof(result.error), "invalid mode: %s", mode->valuestring);
    }

    return result;
}

// =============================================================================
// Main Entry Point
// =============================================================================

CommandResult command_handler_execute(const char *json_str, CVSource *cv_source) {
    CommandResult result = { CMD_UNKNOWN, false, false, "" };

    if (!json_str || !cv_source) {
        snprintf(result.error, sizeof(result.error), "null argument");
        return result;
    }

    // Parse JSON
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        snprintf(result.error, sizeof(result.error),
                 "JSON parse error near: %.30s", error_ptr ? error_ptr : "unknown");
        return result;
    }

    // Get command type
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(json, "cmd");
    if (!cJSON_IsString(cmd) || !cmd->valuestring) {
        snprintf(result.error, sizeof(result.error), "missing 'cmd' field");
        cJSON_Delete(json);
        return result;
    }

    // Dispatch to handler
    if (strcmp(cmd->valuestring, "button") == 0) {
        result = handle_button(json);
    } else if (strcmp(cmd->valuestring, "cv_manual") == 0) {
        result = handle_cv_manual(json, cv_source);
    } else if (strcmp(cmd->valuestring, "cv_lfo") == 0) {
        result = handle_cv_lfo(json, cv_source);
    } else if (strcmp(cmd->valuestring, "cv_envelope") == 0) {
        result = handle_cv_envelope(json, cv_source);
    } else if (strcmp(cmd->valuestring, "cv_gate") == 0) {
        result = handle_cv_gate(json, cv_source);
    } else if (strcmp(cmd->valuestring, "cv_trigger") == 0) {
        result = handle_cv_trigger(cv_source);
    } else if (strcmp(cmd->valuestring, "fault_adc") == 0) {
        result = handle_fault_adc(json);
    } else if (strcmp(cmd->valuestring, "fault_eeprom") == 0) {
        result = handle_fault_eeprom(json);
    } else if (strcmp(cmd->valuestring, "reset") == 0) {
        sim_reset_time();
        cv_source_init(cv_source);
        result.type = CMD_RESET;
        result.success = true;
    } else if (strcmp(cmd->valuestring, "quit") == 0) {
        result.type = CMD_QUIT;
        result.success = true;
        result.should_quit = true;
    } else {
        snprintf(result.error, sizeof(result.error), "unknown command: %s", cmd->valuestring);
    }

    cJSON_Delete(json);
    return result;
}
