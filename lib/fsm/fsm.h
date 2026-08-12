#ifndef GK_FSM_H
#define GK_FSM_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file fsm.h
 * @brief Reusable table-driven finite state machine engine
 *
 * Provides a generic FSM implementation that can be instantiated
 * for different state machines (top-level, mode, menu). Transition
 * tables define behavior declaratively.
 *
 * See ADR-003 and FDP-004 for design rationale.
 */

// Forward declarations
typedef struct State State;
typedef struct Transition Transition;
typedef struct FSM FSM;

/**
 * Special state values for transitions
 */
#define FSM_NO_TRANSITION   0xFF    // Event handled, no state change
#define FSM_ANY_STATE       0xFE    // Matches any current state

/**
 * State definition
 *
 * Each state has optional entry/exit actions and an update function
 * called every main loop iteration while the state is active.
 * Function pointers may be NULL if not needed.
 */
struct State {
    uint8_t id;                 // State identifier
    void (*on_enter)(void);     // Called once on state entry
    void (*on_exit)(void);      // Called once on state exit
    void (*on_update)(void);    // Called every tick while active
};

/**
 * Transition definition
 *
 * Maps (from_state, event) -> (to_state, action).
 * Use FSM_ANY_STATE for from_state to match any current state.
 * Use FSM_NO_TRANSITION for to_state to handle event without changing state.
 */
struct Transition {
    uint8_t from_state;         // Current state (or FSM_ANY_STATE)
    uint8_t event;              // Event that triggers transition
    uint8_t to_state;           // Next state (or FSM_NO_TRANSITION)
    void (*action)(void);       // Action to execute (may be NULL)
};

/**
 * FSM instance
 *
 * Holds pointers to state/transition tables and tracks current state.
 * Multiple FSM instances can coexist (e.g., top-level, mode, menu).
 */
struct FSM {
    const State *states;            // Pointer to state array
    const Transition *transitions;  // Pointer to transition array
    uint8_t num_states;             // Number of states
    uint8_t num_transitions;        // Number of transitions
    uint8_t current_state;          // Current state ID
    uint8_t initial_state;          // Initial state ID (for reset)
    bool active;                    // FSM is processing events
};

/**
 * Initialize an FSM instance.
 *
 * Sets up the FSM with state and transition tables. The FSM starts
 * in the initial state with entry action NOT called (call fsm_start
 * or manually trigger entry if needed).
 *
 * @param fsm           Pointer to FSM struct to initialize
 * @param states        Pointer to state array
 * @param num_states    Number of states in array
 * @param transitions   Pointer to transition array
 * @param num_trans     Number of transitions in array
 * @param initial       Initial state ID
 */
void fsm_init(FSM *fsm, const State *states, uint8_t num_states,
              const Transition *transitions, uint8_t num_trans,
              uint8_t initial);

/**
 * Start the FSM.
 *
 * Sets FSM to active and calls entry action on initial state.
 * Call this after fsm_init to properly enter the initial state.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_start(FSM *fsm);

/**
 * Process an event through the FSM.
 *
 * Searches transition table for matching (current_state, event).
 * If found and to_state is not FSM_NO_TRANSITION:
 *   1. Calls exit action on current state
 *   2. Calls transition action (if any)
 *   3. Updates current state
 *   4. Calls entry action on new state
 *
 * If found and to_state is FSM_NO_TRANSITION:
 *   1. Calls transition action (if any)
 *   2. Returns false (no state change)
 *
 * @param fsm   Pointer to FSM instance
 * @param event Event to process
 * @return      true if state changed, false otherwise
 */
bool fsm_process_event(FSM *fsm, uint8_t event);

/**
 * Run the current state's update function.
 *
 * Should be called every main loop iteration. Does nothing if
 * the current state has no update function or FSM is not active.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_update(FSM *fsm);

/**
 * Reset FSM to initial state.
 *
 * Calls exit action on current state, then entry action on initial state.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_reset(FSM *fsm);

/**
 * Stop the FSM.
 *
 * Calls exit action on current state and sets FSM to inactive.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_stop(FSM *fsm);

/**
 * Get current state ID.
 *
 * @param fsm   Pointer to FSM instance
 * @return      Current state ID
 */
uint8_t fsm_get_state(const FSM *fsm);

/**
 * Force transition to a specific state.
 *
 * Bypasses transition table. Calls exit action on current state
 * and entry action on target state. Useful for context-aware jumps
 * (e.g., menu entry at mode-specific page).
 *
 * @param fsm       Pointer to FSM instance
 * @param state_id  Target state ID
 */
void fsm_set_state(FSM *fsm, uint8_t state_id);

/**
 * Check if FSM is active.
 *
 * @param fsm   Pointer to FSM instance
 * @return      true if FSM is active
 */
bool fsm_is_active(const FSM *fsm);

#endif /* GK_FSM_H */
