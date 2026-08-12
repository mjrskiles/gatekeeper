# FDP-014: FSM Callbacks Utilization and CV Calibration System

## Status

Draft

## Summary

This document proposes two related enhancements to Gatekeeper:

1. **FSM Callback Utilization**: Leverage the existing (but unused) state machine callback infrastructure to add visual polish and improve user experience through menu timeout warnings, idle animations, and mode transition feedback.

2. **CV Input Calibration System**: Implement a lightweight calibration mechanism for the CV input thresholds, with options ranging from software-only auto-learn to hardware trimpot solutions.

Both features benefit from the FSM callback system, as calibration naturally fits as a state with entry/exit/update callbacks.

## Motivation

### FSM Callbacks

The FSM infrastructure in `src/fsm/fsm.c` implements a complete callback system with `on_enter`, `on_exit`, and `on_update` hooks. Currently, all 16 states define these as `NULL`. The callback invocation code exists and is well-tested (15 unit tests), but provides no user-facing benefit.

Removing this infrastructure saves ~342 bytes of flash (per FDP-013). However, strategically utilizing it could significantly improve the tactile feel and usability of the module with minimal additional code.

### CV Calibration

Per ADR-004, the CV input uses software hysteresis with fixed default thresholds (2.5V high, 1.5V low). In practice, Eurorack CV signals vary widely:

| Signal Source | Typical High | Typical Low | Notes |
|---------------|--------------|-------------|-------|
| Mutable Instruments | 5V | 0V | Clean gates |
| Make Noise | 8V+ | 0V | Hot outputs |
| Envelope generators | 0-10V | 0V | Slow attack/release |
| LFOs as clock | Variable | Variable | Depends on wave shape |
| Doepfer standard | 5V | 0V | Reference standard |

Users currently have no way to adjust thresholds if their signals don't trigger reliably or cause false triggers from noise.

## Detailed Design

### Part 1: FSM Callback Utilization

#### 1.1 Callback Priority Matrix

| Callback Use | User Value | Code Cost | Priority |
|--------------|------------|-----------|----------|
| Menu timeout warning | High | ~40 bytes | **P1** |
| Mode entry confirmation | Medium | ~30 bytes | **P2** |
| Idle breathing animation | Low | ~60 bytes | P3 |
| Calibration state lifecycle | High | ~80 bytes | **P1** |

#### 1.2 Menu Timeout Warning (P1)

**Problem**: Menu auto-exits after timeout with no warning, potentially losing user context.

**Solution**: Use `on_update` callback for `TOP_MENU` state to pulse LED as timeout approaches.

```c
// coordinator.c - Menu state definition
static const State top_states[] PROGMEM_ATTR = {
    { TOP_PERFORM, NULL, NULL, on_update_perform },
    { TOP_MENU,    on_enter_menu, NULL, on_update_menu },
};

// Menu update callback - called every main loop while in menu
static void on_update_menu(void) {
    if (!g_coord) return;

    uint32_t elapsed = p_hal->millis() - g_coord->menu_enter_time;
    uint32_t timeout = get_menu_timeout_ms();

    if (elapsed > timeout - MENU_WARNING_MS) {
        // Last 2 seconds: increase LED pulse urgency
        uint8_t urgency = (elapsed - (timeout - MENU_WARNING_MS)) / 100;
        led_feedback_set_urgency(urgency);
    }
}
```

**Visual Behavior**:
- **0 to (timeout-2s)**: Normal LED behavior
- **(timeout-2s) to (timeout-1s)**: LED pulses slowly
- **(timeout-1s) to timeout**: LED pulses rapidly
- **At timeout**: Quick flash, then exit to perform mode

#### 1.3 Mode Entry Visual Confirmation (P2)

**Problem**: When switching modes via B:hold+A:hold gesture, feedback is only the LED color change. Easy to miss.

**Solution**: Use `on_enter` callbacks for mode states to trigger a brief animation.

```c
// Mode state definitions with entry callbacks
static const State mode_states[] PROGMEM_ATTR = {
    { MODE_GATE,    on_enter_gate,    NULL, NULL },
    { MODE_TRIGGER, on_enter_trigger, NULL, NULL },
    { MODE_TOGGLE,  on_enter_toggle,  NULL, NULL },
    { MODE_DIVIDE,  on_enter_divide,  NULL, NULL },
    { MODE_CYCLE,   on_enter_cycle,   NULL, NULL },
};

// Generic mode entry with animation
static void on_enter_mode_generic(ModeState mode) {
    if (!g_coord) return;

    // Re-initialize mode handler with current settings
    mode_handler_init(mode, &g_coord->mode_ctx, g_coord->settings);

    // Trigger entry animation (2 quick pulses in mode color)
    led_feedback_play_animation(ANIM_DOUBLE_PULSE);
}

static void on_enter_gate(void)    { on_enter_mode_generic(MODE_GATE); }
static void on_enter_trigger(void) { on_enter_mode_generic(MODE_TRIGGER); }
// ... etc
```

**Visual Behavior**:
- Two quick LED pulses in the new mode's color
- Total duration: ~200ms
- Provides clear confirmation without blocking CV processing

#### 1.4 Idle Animation (P3)

**Problem**: When no CV activity, module appears "dead" even though it's functioning.

**Solution**: Subtle breathing animation in perform mode after idle timeout.

```c
static void on_update_perform(void) {
    if (!g_coord) return;

    // Check for extended idle (no CV edges for 10 seconds)
    if (p_hal->millis() - g_coord->last_cv_activity > IDLE_THRESHOLD_MS) {
        // Subtle breathing: vary LED brightness sinusoidally
        led_feedback_set_idle_breathing(true);
    } else {
        led_feedback_set_idle_breathing(false);
    }
}
```

**Visual Behavior**:
- After 10 seconds of no CV activity
- LED brightness gently oscillates (80-100% over ~3 second period)
- Immediately stops when CV activity resumes
- Does not affect output behavior

---

### Part 2: CV Calibration System

#### 2.1 Calibration Needs Analysis

| Need | Description | Priority |
|------|-------------|----------|
| Threshold adjustment | Move high/low thresholds for different signals | **Critical** |
| Noise rejection | Widen hysteresis for noisy signals | High |
| Hot signal handling | Higher thresholds for 8V+ sources | Medium |
| Weak signal handling | Lower thresholds for low-amplitude sources | Medium |
| Module variation | Compensate for ADC/circuit tolerance | Low |

#### 2.2 Calibration Options Comparison

| Option | Flash Cost | User Effort | Precision | Hardware Change |
|--------|------------|-------------|-----------|-----------------|
| A: Software auto-learn | ~150 bytes | Low | Good | None |
| B: Menu threshold adjust | ~80 bytes | Medium | Exact | None |
| C: Single trimpot | ~20 bytes | Low | Good | Rev3 PCB |
| D: Dual trimpot | ~30 bytes | Low | Excellent | Rev3 PCB |
| E: Hybrid (A + C) | ~170 bytes | Lowest | Excellent | Rev3 PCB |

#### 2.3 Option A: Software Auto-Learn Calibration

**Concept**: Enter calibration mode, send a representative signal, module learns thresholds.

**FSM Integration**: Add `TOP_CALIBRATE` state with full callback lifecycle.

```c
// New top-level state
typedef enum {
    TOP_PERFORM,
    TOP_MENU,
    TOP_CALIBRATE,  // New
    TOP_COUNT
} TopState;

// Calibration state with callbacks
static const State top_states[] PROGMEM_ATTR = {
    { TOP_PERFORM,   NULL, NULL, on_update_perform },
    { TOP_MENU,      on_enter_menu, NULL, on_update_menu },
    { TOP_CALIBRATE, on_enter_calibrate, on_exit_calibrate, on_update_calibrate },
};

// Transition: secret gesture (e.g., hold both buttons for 5 seconds)
static const Transition top_transitions[] PROGMEM_ATTR = {
    // ... existing transitions ...
    { TOP_PERFORM, EVT_CALIBRATE_START, TOP_CALIBRATE, NULL },
    { TOP_CALIBRATE, EVT_CALIBRATE_DONE, TOP_PERFORM, action_save_calibration },
    { TOP_CALIBRATE, EVT_CALIBRATE_CANCEL, TOP_PERFORM, NULL },
};
```

**Calibration State Machine**:

```
on_enter_calibrate():
    - Initialize sampling: min=255, max=0, sample_count=0
    - Set LED to scanning animation (alternating colors)
    - Start 5-second calibration window

on_update_calibrate():
    - Read ADC value
    - Update min/max observed
    - Increment sample_count
    - Update LED to show progress (color bar)
    - If 5 seconds elapsed OR button press:
        - Compute thresholds
        - Emit EVT_CALIBRATE_DONE

on_exit_calibrate():
    - If sample_count < MIN_SAMPLES:
        - Keep current thresholds (failed calibration)
        - Flash error pattern
    - Else:
        - Compute: high_thresh = max - (max - min) * 0.2
        - Compute: low_thresh = min + (max - min) * 0.2
        - Flash success pattern

action_save_calibration():
    - Store thresholds in EEPROM
    - Apply to CVInput struct
```

**User Flow**:
1. Patch a representative CV signal into Gatekeeper
2. Hold both buttons for 5 seconds to enter calibration mode
3. LED shows "scanning" animation
4. Module observes signal for 5 seconds, tracking min/max
5. LED shows progress as color bar
6. After 5 seconds (or button press), thresholds computed and saved
7. Success: green flash. Failure: red flash.

**Threshold Calculation**:
```
observed_min = 32 (e.g., noise floor at 0.6V)
observed_max = 200 (e.g., signal peak at 3.9V)
range = 200 - 32 = 168

high_threshold = 200 - (168 * 0.2) = 166  (~3.25V)
low_threshold  = 32 + (168 * 0.2) = 66   (~1.3V)
hysteresis = 100 ADC units (~2V) - good noise rejection
```

**Data Structure**:

```c
// Calibration context (temporary, during calibration only)
typedef struct {
    uint8_t min_observed;
    uint8_t max_observed;
    uint16_t sample_count;
    uint32_t start_time;
} CalibrationContext;

// Stored in EEPROM (add to AppSettings)
typedef struct {
    // ... existing fields ...
    uint8_t cv_high_threshold;  // Calibrated high threshold
    uint8_t cv_low_threshold;   // Calibrated low threshold
    uint8_t cv_calibrated;      // 0 = defaults, 1 = calibrated
} AppSettings;
```

#### 2.4 Option B: Menu-Based Threshold Adjustment

**Concept**: Add menu pages for manually setting high/low thresholds.

```c
typedef enum {
    // ... existing pages ...
    PAGE_CV_HIGH_THRESH,  // New: adjust high threshold
    PAGE_CV_LOW_THRESH,   // New: adjust low threshold
    PAGE_COUNT
} MenuPage;
```

**User Flow**:
1. Navigate to CV threshold menu page
2. B-tap cycles through preset threshold values
3. LED brightness indicates current threshold
4. Exit menu to save

**Threshold Presets**:
| Index | High Thresh | Low Thresh | Use Case |
|-------|-------------|------------|----------|
| 0 | 77 (1.5V) | 26 (0.5V) | Low-level signals |
| 1 | 128 (2.5V) | 77 (1.5V) | Default (Doepfer) |
| 2 | 179 (3.5V) | 128 (2.5V) | Hot signals |
| 3 | 230 (4.5V) | 179 (3.5V) | Very hot signals |

**Pros**: Precise control, no learning period
**Cons**: More menu complexity, user must understand thresholds

#### 2.5 Option C: Single Trimpot (Hardware)

**Concept**: Add a single trimpot to adjust threshold position while keeping fixed hysteresis.

**Hardware Change (Rev3 PCB)**:
- Add 10K trimpot between VCC and GND
- Connect wiper to ADC channel (need to sacrifice a pin or add external ADC)
- Alternatively: Use trimpot as voltage divider on CV input path

**Implementation A: Trimpot as Threshold Reference**

```
CV Input ──────┬──────> Comparator ──> Digital Input
               │
Trimpot ───────┴──> ADC (threshold reference)
```

The trimpot sets the comparison threshold directly in hardware. A single comparator (internal analog comparator or external LM393) compares CV to trimpot voltage.

**Implementation B: Trimpot as Signal Attenuator**

```
CV Input ──> Trimpot (attenuator) ──> ADC ──> Software Schmitt
```

The trimpot attenuates hot signals so they fit in the 0-5V ADC range. Software thresholds remain at defaults.

**Pin Options**:
- **Use RESET pin (PB5)**: Requires high-voltage programming after fuse change. Frees one ADC channel.
- **External I2C ADC**: ADS1015 or similar, adds complexity but preserves all pins.
- **Analog mux**: CD4051 to switch between CV and trimpot readings on single ADC channel.

**Firmware (minimal)**:
```c
// Read trimpot position as threshold modifier
void calibration_apply_trimpot(CVInput *cv) {
    uint8_t trim = hal_adc_read(TRIMPOT_CHANNEL);  // 0-255

    // Map trimpot to threshold range
    // Center (128) = default thresholds
    // CCW (0) = low thresholds for weak signals
    // CW (255) = high thresholds for hot signals

    int16_t offset = (int16_t)trim - 128;  // -128 to +127
    offset = offset / 2;  // -64 to +63 ADC units

    cv->high_threshold = CV_DEFAULT_HIGH_THRESHOLD + offset;
    cv->low_threshold = CV_DEFAULT_LOW_THRESHOLD + offset;
}
```

**Pros**: Immediate, tactile adjustment. No menu diving.
**Cons**: Requires PCB revision. Single control adjusts both thresholds equally.

#### 2.6 Option D: Dual Trimpot (Hardware)

**Concept**: Two trimpots for independent high/low threshold control.

**Hardware**:
- Trimpot 1: High threshold
- Trimpot 2: Low threshold (or hysteresis width)

**Pros**: Maximum flexibility
**Cons**: Twice the hardware, twice the ADC channels needed

#### 2.7 Option E: Hybrid (Recommended)

**Concept**: Combine software auto-learn with optional hardware trimpot for real-time adjustment.

**Phase 1 (Rev2 hardware)**: Implement software auto-learn (Option A)
**Phase 2 (Rev3 hardware)**: Add single trimpot as fine-tune on top of learned baseline

```c
void calibration_update_thresholds(CVInput *cv, const AppSettings *settings) {
    // Start with calibrated or default thresholds
    uint8_t base_high = settings->cv_calibrated
        ? settings->cv_high_threshold
        : CV_DEFAULT_HIGH_THRESHOLD;
    uint8_t base_low = settings->cv_calibrated
        ? settings->cv_low_threshold
        : CV_DEFAULT_LOW_THRESHOLD;

    #ifdef HAS_TRIMPOT
    // Apply trimpot offset (if present)
    int8_t offset = get_trimpot_offset();  // -64 to +63
    cv->high_threshold = constrain(base_high + offset, 10, 245);
    cv->low_threshold = constrain(base_low + offset, 5, 240);
    #else
    cv->high_threshold = base_high;
    cv->low_threshold = base_low;
    #endif
}
```

**User Flow with Hybrid**:
1. **Initial setup**: Run auto-learn calibration with typical signal
2. **Fine-tuning**: Adjust trimpot if specific patch needs tweaking
3. **Different source**: Re-run auto-learn, or just use trimpot

---

### Part 3: Integration Architecture

#### 3.1 State Diagram with Calibration

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
                    ▼                                         │
┌──────────────────────────────────┐                         │
│           TOP_PERFORM            │                         │
│  on_update: idle_animation       │                         │
│                                  │                         │
│  ┌─────────┐   ┌─────────┐      │                         │
│  │MODE_GATE│   │MODE_DIV │ ...  │                         │
│  │on_enter │   │on_enter │      │                         │
│  └─────────┘   └─────────┘      │                         │
└──────────────────────────────────┘                         │
        │                   │                                │
        │ EVT_MENU_TOGGLE   │ EVT_CALIBRATE_START           │
        ▼                   ▼                                │
┌──────────────────┐  ┌────────────────────────┐            │
│    TOP_MENU      │  │     TOP_CALIBRATE      │            │
│ on_enter: init   │  │ on_enter: start_sample │            │
│ on_update: warn  │  │ on_update: sample_cv   │────────────┘
│                  │  │ on_exit: compute_thresh│  EVT_CALIBRATE_DONE
│  ┌───────────┐   │  └────────────────────────┘
│  │PAGE_CV_CAL│   │            │
│  └───────────┘   │            │ EVT_CALIBRATE_CANCEL
└──────────────────┘            │
        │                       │
        │ EVT_TIMEOUT/TOGGLE    │
        ▼                       │
        └───────────────────────┴───> TOP_PERFORM
```

#### 3.2 Callback Registration

```c
// coordinator.c - Complete state definitions with callbacks

static const State top_states[] PROGMEM_ATTR = {
    { TOP_PERFORM,   NULL,              NULL,               on_update_perform },
    { TOP_MENU,      on_enter_menu,     NULL,               on_update_menu },
    { TOP_CALIBRATE, on_enter_calibrate, on_exit_calibrate, on_update_calibrate },
};

static const State mode_states[] PROGMEM_ATTR = {
    { MODE_GATE,    on_enter_gate,    on_exit_mode, NULL },
    { MODE_TRIGGER, on_enter_trigger, on_exit_mode, NULL },
    { MODE_TOGGLE,  on_enter_toggle,  on_exit_mode, NULL },
    { MODE_DIVIDE,  on_enter_divide,  on_exit_mode, NULL },
    { MODE_CYCLE,   on_enter_cycle,   on_exit_mode, NULL },
};

static const State menu_states[] PROGMEM_ATTR = {
    { PAGE_GATE_CV,           on_enter_page, NULL, NULL },
    { PAGE_TRIGGER_BEHAVIOR,  on_enter_page, NULL, NULL },
    { PAGE_TRIGGER_PULSE_LEN, on_enter_page, NULL, NULL },
    { PAGE_TOGGLE_BEHAVIOR,   on_enter_page, NULL, NULL },
    { PAGE_DIVIDE_DIVISOR,    on_enter_page, NULL, NULL },
    { PAGE_CYCLE_PATTERN,     on_enter_page, NULL, NULL },
    { PAGE_CV_GLOBAL,         on_enter_page, NULL, NULL },
    { PAGE_MENU_TIMEOUT,      on_enter_page, NULL, NULL },
};
```

#### 3.3 Global Context Solution

The current workaround for callback context uses a global pointer (`g_coord`). A cleaner approach:

**Option 1: Keep global pointer (current approach)**
- Simple, works, tested
- Risk: NULL dereference if callback executes outside coordinator_update()

**Option 2: Extended FSM with user context**
```c
// fsm.h - Add user context to FSM
typedef struct {
    // ... existing fields ...
    void *user_context;  // Opaque pointer to application context
} FSM;

// Callback signature change (breaking)
typedef void (*StateCallback)(void *context);

// Usage
static void on_enter_menu(void *ctx) {
    Coordinator *coord = (Coordinator *)ctx;
    // ...
}
```

**Recommendation**: Keep global pointer for now. The callbacks only execute within `coordinator_update()` scope, so the risk is contained.

---

### Interface Changes

#### New Types

```c
// calibration.h - New file

typedef enum {
    CAL_IDLE,
    CAL_SAMPLING,
    CAL_COMPLETE,
    CAL_FAILED
} CalibrationState;

typedef struct {
    uint8_t min_observed;
    uint8_t max_observed;
    uint16_t sample_count;
    uint32_t start_time;
    CalibrationState state;
} CalibrationContext;

// API
void calibration_start(CalibrationContext *ctx);
void calibration_sample(CalibrationContext *ctx, uint8_t adc_value);
bool calibration_is_complete(const CalibrationContext *ctx);
void calibration_get_thresholds(const CalibrationContext *ctx,
                                 uint8_t *high, uint8_t *low);
```

#### Modified Types

```c
// app_settings.h - Add calibration fields
typedef struct {
    // ... existing fields ...
    uint8_t cv_high_threshold;   // NEW: calibrated high (default: 128)
    uint8_t cv_low_threshold;    // NEW: calibrated low (default: 77)
    uint8_t cv_calibrated;       // NEW: 0=defaults, 1=calibrated
} AppSettings;

// states.h - Add calibration state
typedef enum {
    TOP_PERFORM,
    TOP_MENU,
    TOP_CALIBRATE,  // NEW
    TOP_COUNT
} TopState;

// events.h - Add calibration events
typedef enum {
    // ... existing events ...
    EVT_CALIBRATE_START,   // NEW: enter calibration mode
    EVT_CALIBRATE_DONE,    // NEW: calibration complete
    EVT_CALIBRATE_CANCEL,  // NEW: calibration cancelled
} Event;
```

#### LED Feedback Extensions

```c
// led_feedback.h - Add animation support

typedef enum {
    ANIM_NONE,
    ANIM_DOUBLE_PULSE,      // Mode entry confirmation
    ANIM_BREATH,            // Idle animation
    ANIM_URGENCY_PULSE,     // Menu timeout warning
    ANIM_SCANNING,          // Calibration in progress
    ANIM_PROGRESS_BAR,      // Calibration progress
    ANIM_SUCCESS_FLASH,     // Calibration success
    ANIM_ERROR_FLASH,       // Calibration failure
} AnimationType;

void led_feedback_play_animation(AnimationType anim);
void led_feedback_set_urgency(uint8_t level);
void led_feedback_set_idle_breathing(bool enabled);
void led_feedback_show_progress(uint8_t percent);
```

### Data Structures

```c
// Coordinator extension for calibration
typedef struct {
    // ... existing fields ...
    CalibrationContext calibration;  // NEW: ~6 bytes
    uint32_t last_cv_activity;       // NEW: for idle detection
} Coordinator;
```

### Error Handling

| Scenario | Handling |
|----------|----------|
| Calibration with no signal | Require min sample count; flash error if not met |
| Calibration with constant signal | Require min range (max-min > 20); flash error if not met |
| Threshold out of range | Clamp to valid range (10-245 high, 5-240 low) |
| EEPROM write failure | Keep RAM thresholds, retry on next save |
| Trimpot ADC failure | Fall back to software thresholds |

### Testing Strategy

#### Unit Tests

```c
// test_calibration.h

TEST(CalibrationStartInitializesContext) {
    CalibrationContext ctx;
    calibration_start(&ctx);
    ASSERT_EQ(ctx.min_observed, 255);
    ASSERT_EQ(ctx.max_observed, 0);
    ASSERT_EQ(ctx.sample_count, 0);
}

TEST(CalibrationSamplingTracksMinMax) {
    CalibrationContext ctx;
    calibration_start(&ctx);
    calibration_sample(&ctx, 100);
    calibration_sample(&ctx, 50);
    calibration_sample(&ctx, 200);
    ASSERT_EQ(ctx.min_observed, 50);
    ASSERT_EQ(ctx.max_observed, 200);
}

TEST(CalibrationComputesThresholds) {
    // Given min=50, max=200, range=150
    // Expected: high=170 (200-30), low=80 (50+30)
    CalibrationContext ctx = { .min_observed = 50, .max_observed = 200 };
    uint8_t high, low;
    calibration_get_thresholds(&ctx, &high, &low);
    ASSERT_EQ(high, 170);
    ASSERT_EQ(low, 80);
}

TEST(CalibrationFailsWithInsufficientRange) {
    CalibrationContext ctx = { .min_observed = 100, .max_observed = 110 };
    // Range of 10 is too small
    ASSERT_FALSE(calibration_is_valid(&ctx));
}
```

#### Integration Tests

- Verify calibration mode entry/exit via gesture
- Verify LED animations during calibration
- Verify threshold persistence across power cycle
- Verify menu timeout warning visibility

#### Hardware Tests

- Test with various CV sources (gate, trigger, LFO, envelope)
- Verify noise rejection with calibrated thresholds
- Test trimpot range and linearity (if implemented)

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/core/states.h` | Modify | Add `TOP_CALIBRATE` state |
| `include/events/events.h` | Modify | Add calibration events |
| `include/core/calibration.h` | Create | Calibration context and API |
| `src/core/calibration.c` | Create | Calibration logic |
| `src/core/coordinator.c` | Modify | Add state callbacks, calibration handling |
| `include/config/app_settings.h` | Modify | Add threshold fields |
| `src/app_init.c` | Modify | Validate/migrate calibration settings |
| `include/output/led_feedback.h` | Modify | Add animation API |
| `src/output/led_feedback.c` | Modify | Implement animations |
| `test/unit/core/test_calibration.h` | Create | Calibration unit tests |

## Dependencies

- Existing FSM callback infrastructure (no changes to fsm.c)
- LED feedback system (requires animation extensions)
- EEPROM settings system (add calibration fields)

## Alternatives Considered

### 1. Remove All Callbacks (FDP-013 Option B)

Remove callback infrastructure entirely for flash savings.

**Rejected for this FDP because**: The callbacks enable cleaner architecture for calibration and visual feedback. The ~342 bytes saved is less valuable than the UX improvements.

### 2. Dedicated Calibration Mode Switch

Add a physical switch to enter calibration mode.

**Rejected because**: Adds hardware cost and complexity. Button gesture is sufficient for occasional calibration.

### 3. Serial/USB Calibration Interface

Calibrate via serial commands from computer.

**Rejected because**: Requires USB interface not present on ATtiny85. Adds complexity for marginal benefit.

### 4. Per-Mode Calibration

Store separate thresholds for each mode.

**Rejected because**: Overkill for typical use. Single calibration per CV source is sufficient. Could revisit if users request.

### 5. Continuous Auto-Calibration

Constantly adjust thresholds based on observed signals.

**Rejected because**: Could cause unexpected behavior changes. Explicit calibration is more predictable.

## Open Questions

1. **Calibration gesture**: Hold both buttons for 5 seconds? Or add a menu page to start calibration? The gesture is more discoverable via documentation; menu page adds clutter.

2. **Trimpot location**: If adding hardware trimpot, front panel (user accessible) or PCB-only (set once at build)? Front panel adds cost but enables patch-specific tuning.

3. **Calibration feedback**: Is LED-only feedback sufficient, or should we also change output behavior during calibration (e.g., output follows input state)?

4. **Multiple calibration profiles**: Should we support saving multiple threshold presets? Adds complexity but useful for users with diverse gear.

5. **Idle animation opt-out**: Should idle breathing be a user setting? Some users may prefer static LEDs.

6. **Rev3 trimpot pin**: If using trimpot, which pin? Options:
   - PB5 (RESET) - requires HVSP programming
   - External I2C ADC - adds component
   - Time-share ADC3 with CV input - adds complexity

## Implementation Checklist

### Phase 1: FSM Callbacks (P1 + P2)
- [ ] Implement `on_update_menu()` for timeout warning
- [ ] Add urgency animation to LED feedback
- [ ] Implement `on_enter_*()` for mode states
- [ ] Add double-pulse animation to LED feedback
- [ ] Update state tables with callback pointers
- [ ] Test menu timeout warning visibility
- [ ] Test mode entry confirmation feel

### Phase 2: Calibration Core
- [ ] Create `calibration.h` and `calibration.c`
- [ ] Implement `CalibrationContext` and sampling logic
- [ ] Add threshold computation with validation
- [ ] Add `TOP_CALIBRATE` state and transitions
- [ ] Implement calibration state callbacks
- [ ] Add calibration fields to `AppSettings`
- [ ] Implement EEPROM persistence
- [ ] Add calibration gesture detection

### Phase 3: Calibration UX
- [ ] Add scanning animation for calibration
- [ ] Add progress bar animation
- [ ] Add success/failure flash patterns
- [ ] Test with various CV sources
- [ ] Document calibration procedure

### Phase 4: Idle Animation (P3)
- [ ] Implement `on_update_perform()` for idle detection
- [ ] Add breathing animation to LED feedback
- [ ] Add idle timeout tracking
- [ ] Test animation trigger/cancel

### Phase 5: Hardware Trimpot (Future/Optional)
- [ ] Design trimpot circuit for Rev3
- [ ] Select ADC channel strategy
- [ ] Implement trimpot reading
- [ ] Test threshold offset calculation
- [ ] Document trimpot adjustment procedure
