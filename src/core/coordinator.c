#include "core/coordinator.h"
#include "modes/mode_handlers.h"
#include "hardware/hal_interface.h"
#include "input/cv_input.h"
#include "config/mode_config.h"
#include "utility/progmem.h"
#include <stddef.h>

/**
 * @file coordinator.c
 * @brief Gatekeeper application coordinator implementation
 *
 * Implements the three-level FSM hierarchy and event routing.
 */

// =============================================================================
// Forward declarations for action functions
// =============================================================================

static void action_enter_menu(void);
static void action_exit_menu(void);
static void action_exit_menu_on_hold(void);
static void action_next_mode(void);
static void action_next_page(void);
static void action_cycle_value(void);

// Global pointer for action functions (set during update)
static Coordinator *g_coord = NULL;

// =============================================================================
// State definitions
// =============================================================================

// Top-level states (PERFORM / MENU)
static const State top_states[] PROGMEM_ATTR = {
    { TOP_PERFORM, NULL, NULL, NULL },
    { TOP_MENU,    NULL, NULL, NULL },
};

// Mode states (signal processing modes)
// Entry/exit/update functions will be added in AP-003 (mode handlers)
static const State mode_states[] PROGMEM_ATTR = {
    { MODE_GATE,    NULL, NULL, NULL },
    { MODE_TRIGGER, NULL, NULL, NULL },
    { MODE_TOGGLE,  NULL, NULL, NULL },
    { MODE_DIVIDE,  NULL, NULL, NULL },
    { MODE_CYCLE,   NULL, NULL, NULL },
};

// Menu page states
// Entry functions could display page indicator, exit could save
static const State menu_states[] PROGMEM_ATTR = {
    { PAGE_GATE_CV,           NULL, NULL, NULL },
    { PAGE_TRIGGER_BEHAVIOR,  NULL, NULL, NULL },
    { PAGE_TRIGGER_PULSE_LEN, NULL, NULL, NULL },
    { PAGE_TOGGLE_BEHAVIOR,   NULL, NULL, NULL },
    { PAGE_DIVIDE_DIVISOR,    NULL, NULL, NULL },
    { PAGE_CYCLE_PATTERN,     NULL, NULL, NULL },
    { PAGE_CV_GLOBAL,         NULL, NULL, NULL },
    { PAGE_MENU_TIMEOUT,      NULL, NULL, NULL },
};

// =============================================================================
// Transition tables
// =============================================================================

// Top-level transitions
static const Transition top_transitions[] PROGMEM_ATTR = {
    // Enter menu: A:hold + B:hold gesture
    { TOP_PERFORM, EVT_MENU_TOGGLE, TOP_MENU,    action_enter_menu },

    // Exit menu: A:hold (solo) fires immediately on hold threshold
    { TOP_MENU,    EVT_A_HOLD,      TOP_PERFORM, action_exit_menu_on_hold },
    { TOP_MENU,    EVT_TIMEOUT,     TOP_PERFORM, action_exit_menu },
};

// Mode transitions (mode change gesture works from any mode in PERFORM)
static const Transition mode_transitions[] PROGMEM_ATTR = {
    // A:hold (solo) release = advance to next mode
    { FSM_ANY_STATE, EVT_MODE_NEXT, FSM_NO_TRANSITION, action_next_mode },
};

// Menu transitions (navigation within menu)
static const Transition menu_transitions[] PROGMEM_ATTR = {
    // A:tap = next page
    { FSM_ANY_STATE, EVT_A_TAP, FSM_NO_TRANSITION, action_next_page },

    // B:tap = cycle value on current page
    { FSM_ANY_STATE, EVT_B_TAP, FSM_NO_TRANSITION, action_cycle_value },
};

// =============================================================================
// Action functions
// =============================================================================

static void action_enter_menu(void) {
    if (!g_coord) return;

    // Save current mode for context-aware page selection
    g_coord->menu_entry_mode = fsm_get_state(&g_coord->mode_fsm);
    g_coord->menu_enter_time = p_hal->millis();
    g_coord->last_activity = g_coord->menu_enter_time;

    // Jump to mode-relevant page
    MenuPage start_page = mode_to_start_page((ModeState)g_coord->menu_entry_mode);
    fsm_set_state(&g_coord->menu_fsm, start_page);
}

static void action_exit_menu(void) {
    if (!g_coord) return;

    // Save settings on menu exit
    if (g_coord->settings) {
        g_coord->settings->mode = fsm_get_state(&g_coord->mode_fsm);
        app_init_save_settings(g_coord->settings);
    }
}

static void action_exit_menu_on_hold(void) {
    if (!g_coord) return;

    // Save settings on menu exit
    if (g_coord->settings) {
        g_coord->settings->mode = fsm_get_state(&g_coord->mode_fsm);
        app_init_save_settings(g_coord->settings);
    }

    // Set compound flag to prevent EVT_MODE_NEXT from firing on release
    // (we don't want menu exit to also trigger mode change)
    g_coord->events.ext_status |= EP_COMPOUND_FIRED;
}

static void action_next_mode(void) {
    if (!g_coord) return;

    uint8_t current = fsm_get_state(&g_coord->mode_fsm);
    uint8_t next = (current + 1) % MODE_COUNT;
    fsm_set_state(&g_coord->mode_fsm, next);

    // Initialize context for new mode with current settings
    mode_handler_init(next, &g_coord->mode_ctx, g_coord->settings);

    // Update activity timestamp
    g_coord->last_activity = p_hal->millis();
}

static void action_next_page(void) {
    if (!g_coord) return;

    uint8_t current = fsm_get_state(&g_coord->menu_fsm);
    uint8_t next = (current + 1) % PAGE_COUNT;
    fsm_set_state(&g_coord->menu_fsm, next);

    // Update activity timestamp
    g_coord->last_activity = p_hal->millis();
}

static void action_cycle_value(void) {
    if (!g_coord || !g_coord->settings) return;

    MenuPage page = (MenuPage)fsm_get_state(&g_coord->menu_fsm);
    ModeState current_mode = (ModeState)fsm_get_state(&g_coord->mode_fsm);
    bool reinit_needed = false;

    switch (page) {
        case PAGE_GATE_CV:
            g_coord->settings->gate_a_mode =
                (g_coord->settings->gate_a_mode + 1) % GATE_A_MODE_COUNT;
            if (current_mode == MODE_GATE) reinit_needed = true;
            break;

        case PAGE_TRIGGER_BEHAVIOR:
            g_coord->settings->trigger_edge =
                (g_coord->settings->trigger_edge + 1) % TRIGGER_EDGE_COUNT;
            if (current_mode == MODE_TRIGGER) reinit_needed = true;
            break;

        case PAGE_TRIGGER_PULSE_LEN:
            g_coord->settings->trigger_pulse_idx =
                (g_coord->settings->trigger_pulse_idx + 1) % TRIGGER_PULSE_COUNT;
            if (current_mode == MODE_TRIGGER) reinit_needed = true;
            break;

        case PAGE_TOGGLE_BEHAVIOR:
            g_coord->settings->toggle_edge =
                (g_coord->settings->toggle_edge + 1) % TOGGLE_EDGE_COUNT;
            if (current_mode == MODE_TOGGLE) reinit_needed = true;
            break;

        case PAGE_DIVIDE_DIVISOR:
            g_coord->settings->divide_divisor_idx =
                (g_coord->settings->divide_divisor_idx + 1) % DIVIDE_DIVISOR_COUNT;
            if (current_mode == MODE_DIVIDE) reinit_needed = true;
            break;

        case PAGE_CYCLE_PATTERN:
            g_coord->settings->cycle_tempo_idx =
                (g_coord->settings->cycle_tempo_idx + 1) % CYCLE_TEMPO_COUNT;
            if (current_mode == MODE_CYCLE) reinit_needed = true;
            break;

        case PAGE_CV_GLOBAL:
        case PAGE_MENU_TIMEOUT:
        default:
            // Not implemented yet - no cycling action
            break;
    }

    // Reinitialize mode handler if current mode's setting changed
    if (reinit_needed) {
        mode_handler_init(current_mode, &g_coord->mode_ctx, g_coord->settings);
    }

    // Update activity timestamp for menu timeout
    g_coord->last_activity = p_hal->millis();
}

// =============================================================================
// Public API
// =============================================================================

void coordinator_init(Coordinator *coord, AppSettings *settings) {
    if (!coord) return;

    coord->settings = settings;
    coord->menu_entry_mode = MODE_GATE;
    coord->menu_enter_time = 0;
    coord->last_activity = 0;
    coord->output_state = false;

    // Initialize event processor
    event_processor_init(&coord->events);

    // Initialize CV input with hysteresis (per ADR-004)
    cv_input_init(&coord->cv_input);

    // Initialize mode handler context (default to gate mode)
    mode_handler_init(MODE_GATE, &coord->mode_ctx, settings);

    // Initialize top-level FSM
    fsm_init(&coord->top_fsm,
             top_states, TOP_STATE_COUNT,
             top_transitions, sizeof(top_transitions) / sizeof(top_transitions[0]),
             TOP_PERFORM);

    // Initialize mode FSM
    fsm_init(&coord->mode_fsm,
             mode_states, MODE_COUNT,
             mode_transitions, sizeof(mode_transitions) / sizeof(mode_transitions[0]),
             MODE_GATE);

    // Initialize menu FSM
    fsm_init(&coord->menu_fsm,
             menu_states, PAGE_COUNT,
             menu_transitions, sizeof(menu_transitions) / sizeof(menu_transitions[0]),
             PAGE_GATE_CV);
}

void coordinator_start(Coordinator *coord) {
    if (!coord) return;

    // Start all FSMs
    fsm_start(&coord->top_fsm);
    fsm_start(&coord->mode_fsm);
    fsm_start(&coord->menu_fsm);

    coord->last_activity = p_hal->millis();
}

void coordinator_update(Coordinator *coord) {
    if (!coord) return;

    // Set global pointer for action functions
    g_coord = coord;

    // Read CV input via ADC and apply hysteresis (per ADR-004)
    uint8_t cv_adc = p_hal->adc_read(CV_ADC_CHANNEL);
    bool cv_state = cv_input_update(&coord->cv_input, cv_adc);

    // Build input state from HAL (buttons are active-low: pressed = LOW)
    EventInput input = {
        .button_a = !p_hal->read_pin(p_hal->button_a_pin),
        .button_b = !p_hal->read_pin(p_hal->button_b_pin),
        .cv_in = cv_state,
        .current_time = p_hal->millis()
    };

    // Process input to get event
    Event event = event_processor_update(&coord->events, &input);

    // Route event to appropriate FSM based on current top-level state
    if (event != EVT_NONE) {
        TopState top_state = (TopState)fsm_get_state(&coord->top_fsm);

        // Reset menu timeout on any button activity while in menu
        if (top_state == TOP_MENU) {
            coord->last_activity = p_hal->millis();
        }

        // Top-level transitions (menu enter/exit)
        bool handled = fsm_process_event(&coord->top_fsm, event);

        if (!handled) {
            if (top_state == TOP_PERFORM) {
                // In perform mode: route to mode FSM
                fsm_process_event(&coord->mode_fsm, event);
            } else {
                // In menu mode: route to menu FSM
                fsm_process_event(&coord->menu_fsm, event);
            }
        }
    }

    // Check menu timeout
    if (fsm_get_state(&coord->top_fsm) == TOP_MENU) {
        uint32_t elapsed = p_hal->millis() - coord->last_activity;
        if (elapsed >= MENU_TIMEOUT_MS) {
            fsm_process_event(&coord->top_fsm, EVT_TIMEOUT);
        }
    }

    // Update current mode (run mode handler)
    // Signal processing runs in both PERFORM and MENU modes
    // - PERFORM: CV input OR button B (and button A for some modes)
    // - MENU: CV input only (buttons used for menu navigation)
    ModeState mode = (ModeState)fsm_get_state(&coord->mode_fsm);
    bool input_state;

    if (fsm_get_state(&coord->top_fsm) == TOP_PERFORM) {
        // In perform mode: CV OR button B
        // Suppress B trigger when A is held (menu-enter gesture in progress)
        bool b_triggers = event_processor_b_pressed(&coord->events) &&
                          !event_processor_a_pressed(&coord->events);
        input_state = cv_state || b_triggers;

        // In Gate mode with gate_a_mode enabled, button A also triggers
        // (only when not already holding for menu gesture)
        if (mode == MODE_GATE && coord->settings &&
            coord->settings->gate_a_mode == GATE_A_MODE_MANUAL) {
            input_state = input_state || event_processor_a_pressed(&coord->events);
        }
    } else {
        // In menu mode: CV only (buttons reserved for menu navigation)
        input_state = cv_state;
    }

    mode_handler_process(mode, &coord->mode_ctx, input_state, &coord->output_state);

    // Clear global pointer
    g_coord = NULL;
}

TopState coordinator_get_top_state(const Coordinator *coord) {
    if (!coord) return TOP_PERFORM;
    return (TopState)fsm_get_state(&coord->top_fsm);
}

ModeState coordinator_get_mode(const Coordinator *coord) {
    if (!coord) return MODE_GATE;
    return (ModeState)fsm_get_state(&coord->mode_fsm);
}

void coordinator_set_mode(Coordinator *coord, ModeState mode) {
    if (!coord) return;
    if (mode >= MODE_COUNT) return;
    fsm_set_state(&coord->mode_fsm, mode);
    mode_handler_init(mode, &coord->mode_ctx, coord->settings);
}

bool coordinator_in_menu(const Coordinator *coord) {
    if (!coord) return false;
    return fsm_get_state(&coord->top_fsm) == TOP_MENU;
}

MenuPage coordinator_get_page(const Coordinator *coord) {
    if (!coord) return PAGE_GATE_CV;
    return (MenuPage)fsm_get_state(&coord->menu_fsm);
}

bool coordinator_get_output(const Coordinator *coord) {
    if (!coord) return false;
    return coord->output_state;
}

void coordinator_get_led_feedback(const Coordinator *coord, LEDFeedback *feedback) {
    if (!coord || !feedback) return;

    ModeState mode = (ModeState)fsm_get_state(&coord->mode_fsm);
    mode_handler_get_led(mode, &coord->mode_ctx, feedback);

    // Add application state for LED controller transitions
    feedback->current_mode = mode;
    feedback->current_page = (uint8_t)fsm_get_state(&coord->menu_fsm);
    feedback->in_menu = (fsm_get_state(&coord->top_fsm) == TOP_MENU);

    // Add menu value feedback
    feedback->setting_value = 0;
    feedback->setting_max = 1;

    if (coord->settings) {
        MenuPage page = (MenuPage)feedback->current_page;
        switch (page) {
            case PAGE_GATE_CV:
                feedback->setting_value = coord->settings->gate_a_mode;
                feedback->setting_max = GATE_A_MODE_COUNT;
                break;
            case PAGE_TRIGGER_BEHAVIOR:
                feedback->setting_value = coord->settings->trigger_edge;
                feedback->setting_max = TRIGGER_EDGE_COUNT;
                break;
            case PAGE_TRIGGER_PULSE_LEN:
                feedback->setting_value = coord->settings->trigger_pulse_idx;
                feedback->setting_max = TRIGGER_PULSE_COUNT;
                break;
            case PAGE_TOGGLE_BEHAVIOR:
                feedback->setting_value = coord->settings->toggle_edge;
                feedback->setting_max = TOGGLE_EDGE_COUNT;
                break;
            case PAGE_DIVIDE_DIVISOR:
                feedback->setting_value = coord->settings->divide_divisor_idx;
                feedback->setting_max = DIVIDE_DIVISOR_COUNT;
                break;
            case PAGE_CYCLE_PATTERN:
                feedback->setting_value = coord->settings->cycle_tempo_idx;
                feedback->setting_max = CYCLE_TEMPO_COUNT;
                break;
            default:
                break;
        }
    }
}

bool coordinator_get_cv_state(const Coordinator *coord) {
    if (!coord) return false;
    return cv_input_get_state(&coord->cv_input);
}
