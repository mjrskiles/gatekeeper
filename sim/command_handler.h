#ifndef GK_SIM_COMMAND_HANDLER_H
#define GK_SIM_COMMAND_HANDLER_H

#include "cv_source.h"
#include <stdbool.h>

/**
 * @file command_handler.h
 * @brief JSON command parser for socket protocol
 *
 * Parses NDJSON commands and applies them to simulator state.
 */

// Command types
typedef enum {
    CMD_UNKNOWN,
    CMD_BUTTON,
    CMD_CV_MANUAL,
    CMD_CV_LFO,
    CMD_CV_ENVELOPE,
    CMD_CV_GATE,
    CMD_CV_TRIGGER,
    CMD_CV_WAVETABLE,
    CMD_FAULT_ADC,      // FDP-016: ADC fault injection
    CMD_FAULT_EEPROM,   // FDP-016 Phase 3: EEPROM fault injection
    CMD_RESET,
    CMD_QUIT
} CommandType;

// Command result
typedef struct {
    CommandType type;
    bool success;
    bool should_quit;
    char error[128];
} CommandResult;

/**
 * Parse and execute a JSON command.
 *
 * @param json      JSON command string (without newline)
 * @param cv_source CV source to modify
 * @return Command result with type, success status, and any error
 */
CommandResult command_handler_execute(const char *json, CVSource *cv_source);

/**
 * Get command type name for logging.
 */
const char* command_type_str(CommandType type);

#endif /* GK_SIM_COMMAND_HANDLER_H */
