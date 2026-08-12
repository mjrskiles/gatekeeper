#ifndef GK_MODES_MODE_HANDLERS_H
#define GK_MODES_MODE_HANDLERS_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration to avoid circular dependency
typedef struct AppSettings AppSettings;

/**
 * @file mode_handlers.h
 * @brief Mode handler contexts and LED feedback types
 *
 * Defines the per-mode context structs that hold mode-specific state,
 * and the LED feedback structure for Neopixel output.
 *
 * Mode handlers are implemented as switch-based dispatch in coordinator.c,
 * not as function pointers (per Klaus's recommendation for 5 fixed modes).
 *
 * See AP-003 for implementation details.
 */

// =============================================================================
// Timing constants
// =============================================================================

// Trigger mode pulse durations (configurable via menu)
#define TRIGGER_PULSE_1MS     1
#define TRIGGER_PULSE_10MS    10
#define TRIGGER_PULSE_20MS    20
#define TRIGGER_PULSE_50MS    50
#define TRIGGER_PULSE_DEFAULT TRIGGER_PULSE_10MS

// Divide mode divisors
#define DIVIDE_MIN            2
#define DIVIDE_MAX            24
#define DIVIDE_DEFAULT        2

// Cycle mode timing
#define CYCLE_DEFAULT_BPM     80
#define CYCLE_BPM_TO_PERIOD_MS(bpm) (60000 / (bpm))  // Full cycle period
#define CYCLE_DEFAULT_PERIOD_MS CYCLE_BPM_TO_PERIOD_MS(CYCLE_DEFAULT_BPM)  // 750ms

// Divide/Cycle output pulse duration
#define OUTPUT_PULSE_MS       10

// =============================================================================
// LED Feedback
// =============================================================================

/**
 * LED feedback state for Neopixel output.
 *
 * Gatekeeper Rev2 has two Neopixels in series on PB0:
 * - LED 1 (mode indicator): Shows current mode color
 * - LED 2 (activity indicator): Shows output state / activity
 *
 * Also includes application state info for LEDFeedbackController
 * to handle menu enter/exit transitions automatically.
 */
typedef struct {
    // Mode indicator LED (LED 1)
    uint8_t mode_r;
    uint8_t mode_g;
    uint8_t mode_b;

    // Activity indicator LED (LED 2)
    uint8_t activity_r;
    uint8_t activity_g;
    uint8_t activity_b;
    uint8_t activity_brightness;  // 0-255, for pulsing effects

    // Application state (for LED controller transitions)
    uint8_t current_mode;         // Current mode (MODE_GATE, etc.)
    uint8_t current_page;         // Current menu page (when in menu)
    bool in_menu;                 // Currently in menu mode

    // Menu value feedback (for activity LED in menu)
    uint8_t setting_value;        // Current setting index (0-based)
    uint8_t setting_max;          // Max value + 1 (count of options)
} LEDFeedback;

// Mode colors (approximate, can be tuned)
#define LED_COLOR_GATE_R      0
#define LED_COLOR_GATE_G      255
#define LED_COLOR_GATE_B      0

#define LED_COLOR_TRIGGER_R   0
#define LED_COLOR_TRIGGER_G   128
#define LED_COLOR_TRIGGER_B   255

#define LED_COLOR_TOGGLE_R    255
#define LED_COLOR_TOGGLE_G    64
#define LED_COLOR_TOGGLE_B    0

#define LED_COLOR_DIVIDE_R    255
#define LED_COLOR_DIVIDE_G    0
#define LED_COLOR_DIVIDE_B    255

#define LED_COLOR_CYCLE_R     255
#define LED_COLOR_CYCLE_G     255
#define LED_COLOR_CYCLE_B     0

// Activity LED (white when on)
#define LED_ACTIVITY_R        255
#define LED_ACTIVITY_G        255
#define LED_ACTIVITY_B        255

// =============================================================================
// Mode Context Structs
// =============================================================================

/**
 * Gate mode context
 *
 * Simple pass-through: output follows input.
 */
typedef struct {
    bool output_state;
} GateContext;

/**
 * Trigger mode context
 *
 * Generates fixed-duration pulse on rising edge.
 */
typedef struct {
    bool output_state;
    bool last_input;
    uint32_t pulse_start;
    uint16_t pulse_duration_ms;
} TriggerContext;

/**
 * Toggle mode context
 *
 * Flip-flop: each rising edge toggles output state.
 */
typedef struct {
    bool output_state;
    bool last_input;
} ToggleContext;

/**
 * Divide mode context
 *
 * Clock divider: outputs pulse every N input pulses.
 */
typedef struct {
    bool output_state;
    bool last_input;
    uint8_t counter;
    uint8_t divisor;
    uint32_t pulse_start;
} DivideContext;

/**
 * Cycle mode context
 *
 * Internal clock generator at fixed BPM.
 * Output toggles at half-period intervals.
 */
typedef struct {
    bool output_state;
    bool running;
    uint32_t last_toggle;
    uint16_t period_ms;       // Full cycle period
    uint8_t phase;            // 0-255 for LED brightness animation
} CycleContext;

/**
 * Combined mode context union
 *
 * Only one mode is active at a time, so contexts can share memory.
 * This saves RAM compared to allocating all contexts separately.
 */
typedef union {
    GateContext gate;
    TriggerContext trigger;
    ToggleContext toggle;
    DivideContext divide;
    CycleContext cycle;
} ModeContext;

// =============================================================================
// Handler function prototypes
// =============================================================================

/**
 * Initialize mode context with settings from EEPROM.
 *
 * Called when switching to a new mode. Uses settings to configure
 * mode-specific parameters (pulse length, divisor, tempo, etc.)
 *
 * @param mode      Mode to initialize
 * @param ctx       Context union to initialize
 * @param settings  Application settings (may be NULL for defaults)
 */
void mode_handler_init(uint8_t mode, ModeContext *ctx, const AppSettings *settings);

/**
 * Process input through current mode handler.
 *
 * @param mode    Current mode
 * @param ctx     Mode context
 * @param input   Current input state (button/CV)
 * @param output  Pointer to store output state
 * @return        true if output changed
 */
bool mode_handler_process(uint8_t mode, ModeContext *ctx, bool input, bool *output);

/**
 * Get LED feedback for current mode state.
 *
 * @param mode      Current mode
 * @param ctx       Mode context
 * @param feedback  Pointer to LED feedback struct to fill
 */
void mode_handler_get_led(uint8_t mode, const ModeContext *ctx, LEDFeedback *feedback);

#endif /* GK_MODES_MODE_HANDLERS_H */
