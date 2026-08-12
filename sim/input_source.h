#ifndef GK_SIM_INPUT_SOURCE_H
#define GK_SIM_INPUT_SOURCE_H

#include <stdbool.h>
#include <stdint.h>
#include "sim_state.h"

/**
 * @file input_source.h
 * @brief Abstract input source interface for simulator
 *
 * Both interactive (keyboard) and scripted input implement this interface,
 * allowing the main loop to be agnostic to input source.
 */

// Forward declaration
typedef struct InputSource InputSource;

/**
 * Input source interface
 *
 * Implementations:
 * - KeyboardInput: Interactive terminal input
 * - ScriptInput: Reads from script file
 */
struct InputSource {
    /**
     * Update input state for current time.
     * Called each tick of the main loop.
     *
     * @param current_time_ms  Current simulation time
     * @return true to continue, false to quit
     */
    bool (*update)(InputSource *self, uint32_t current_time_ms);

    /**
     * Check if simulation should run in real-time.
     * Keyboard input runs real-time (1ms tick with usleep).
     * Scripts run as fast as possible (no pacing delay).
     *
     * @return true for real-time pacing, false for no pacing
     */
    bool (*is_realtime)(InputSource *self);

    /**
     * Check if script had any assertion failures.
     * Only meaningful for script input sources.
     *
     * @return true if any assertions failed
     */
    bool (*has_failed)(InputSource *self);

    /**
     * Cleanup resources.
     */
    void (*cleanup)(InputSource *self);

    // Private data for implementation
    void *ctx;
};

/**
 * Create keyboard input source (interactive mode)
 *
 * @param sim_state  SimState pointer for UI controls (F/L keys), can be NULL
 */
InputSource* input_source_keyboard_create(SimState *sim_state);

/**
 * Create script input source
 *
 * @param filename  Path to script file
 * @return InputSource or NULL on error
 */
InputSource* input_source_script_create(const char *filename);

#endif /* GK_SIM_INPUT_SOURCE_H */
