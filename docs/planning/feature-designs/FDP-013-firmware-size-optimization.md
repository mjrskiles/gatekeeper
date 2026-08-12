# FDP-013: Firmware Size Optimization

## Status

Partially Implemented (2025-12-21)

**Implemented:**
- Phase 1.1: LTO enabled - saved 844 bytes (94% → 84%)
- Phase 2.2: Settings validation refactored to table-driven - saved 28 bytes

**Skipped (intentionally):**
- Phase 1.2: Conditional compilation - no savings (functions don't exist in prod HAL)
- Phase 2.1: FSM simplification - preserving callback infrastructure for future use
- Phase 2.3: LED feedback switch - readability more important than ~50 byte savings
- Phase 3.1: Color palette - readability more important for reference project

**Result:** 872 bytes saved total, 83.7% flash usage (1338 bytes free)

## Summary

Reduce firmware flash usage from ~95% to <85% through a series of targeted optimizations including Link-Time Optimization (LTO), FSM simplification, conditional compilation of test functions, LED color data consolidation, and validation code refactoring. Combined savings estimated at 550-1200 bytes.

## Motivation

The ATtiny85 has 8KB (8192 bytes) of flash memory. Current firmware usage is at approximately 95%, leaving only ~400 bytes for future features. This is critically tight for an embedded project with potential enhancements planned.

**Current state:**
- Flash: ~7780 bytes used (95%)
- RAM: ~200 bytes used (39%)
- Headroom: ~400 bytes flash, ~300 bytes RAM

**Target state:**
- Flash: <7000 bytes (85%)
- Headroom: >1000 bytes for future features

The optimizations in this FDP are prioritized by impact-to-effort ratio and categorized into phases for incremental implementation.

## Detailed Design

### Overview

The optimization strategy is divided into three phases:

| Phase | Focus | Estimated Savings | Risk |
|-------|-------|-------------------|------|
| Phase 1 | Build system changes (LTO, conditional compile) | 250-600 bytes | Low |
| Phase 2 | Code simplification (FSM, validation) | 150-400 bytes | Medium |
| Phase 3 | Data consolidation (LED colors) | 80-200 bytes | Low |

### Phase 1: Build System Optimizations

#### 1.1 Enable Link-Time Optimization (LTO)

**Estimated savings: 200-500 bytes**

LTO enables cross-module optimization, allowing the compiler to inline functions across translation units and eliminate dead code that survives per-file compilation.

```cmake
# CMakeLists.txt changes (lines 97-99)
set(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -fshort-enums -flto")
set(CMAKE_EXE_LINKER_FLAGS "-mmcu=${MCU} -Wl,--gc-sections -s -flto")
```

**Verification:**
```bash
# Before LTO
avr-size --format=avr --mcu=attiny85 gatekeeper

# After LTO - compare sizes
```

**Risks:**
- Slightly longer compile times
- Rare edge cases with inline assembly (test neopixel driver)

#### 1.2 Conditional Compilation of Test Functions

**Estimated savings: 50-100 bytes**

Test-only functions in `hal.c` are compiled into production firmware but never called:

```c
// hal.c:237-254 - Only used by test harness
void hal_advance_time(uint32_t ms);  // ~30 bytes
void hal_reset_time(void);           // ~20 bytes
```

**Implementation:**

```c
// hal.h - Add guards
#if !defined(NDEBUG) || defined(TEST_BUILD)
void hal_advance_time(uint32_t ms);
void hal_reset_time(void);
#endif

// hal.c - Wrap implementations
#if !defined(NDEBUG) || defined(TEST_BUILD)
void hal_advance_time(uint32_t ms) {
    // existing implementation
}

void hal_reset_time(void) {
    // existing implementation
}
#endif
```

```cmake
# CMakeLists.txt - Add NDEBUG for release builds
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG")
```

### Phase 2: Code Simplification

#### 2.1 FSM Infrastructure Reduction

**Estimated savings: 150-300 bytes**

The generic FSM engine (`fsm.c`) provides features that are not used by this application. All states have NULL callbacks:

```c
// coordinator.c - All state definitions have NULL callbacks
static const State top_states[] PROGMEM_ATTR = {
    { TOP_PERFORM, NULL, NULL, NULL },  // on_enter, on_exit, on_update
    { TOP_MENU,    NULL, NULL, NULL },
};
```

**Option A: Remove unused callback infrastructure (Conservative)**

Remove `on_update` callback entirely since it's never used:

```c
// fsm.h - Simplified State struct
typedef struct {
    uint8_t id;
    void (*on_enter)(void);
    void (*on_exit)(void);
    // Remove: void (*on_update)(void);
} State;

// fsm.c - Remove fsm_update() function entirely (~30 lines)
```

**Option B: Remove all callbacks (Aggressive)**

Since all callbacks are NULL, remove the entire callback system:

```c
// fsm.h - Minimal State struct
typedef struct {
    uint8_t id;
    // All callbacks removed - actions only in Transition
} State;

// Or eliminate State struct entirely, just track state IDs
```

**Option C: Inline FSM logic (Maximum savings)**

Replace the generic FSM engine with direct state machine logic in coordinator:

```c
// coordinator.c - Direct implementation
static uint8_t top_state = TOP_PERFORM;
static uint8_t mode_state = MODE_GATE;
static uint8_t menu_state = PAGE_GATE_CV;

void coordinator_update(Coordinator *coord) {
    // Direct state transitions instead of table lookups
    if (event == EVT_MENU_TOGGLE) {
        if (top_state == TOP_PERFORM) {
            top_state = TOP_MENU;
            action_enter_menu();
        } else {
            top_state = TOP_PERFORM;
            action_exit_menu();
        }
    }
    // ...
}
```

**Recommendation:** Start with Option A, measure savings, then consider B or C.

#### 2.2 Settings Validation Refactoring

**Estimated savings: 30-60 bytes**

Current validation in `app_init.c` uses 7 separate if statements:

```c
// app_init.c:180-200 - Current implementation
if (settings->mode >= MODE_COUNT) return false;
if (settings->trigger_pulse_idx >= TRIGGER_PULSE_COUNT) return false;
if (settings->trigger_edge >= TRIGGER_EDGE_COUNT) return false;
if (settings->divide_divisor_idx >= DIVIDE_DIVISOR_COUNT) return false;
if (settings->cycle_tempo_idx >= CYCLE_TEMPO_COUNT) return false;
if (settings->toggle_edge >= TOGGLE_EDGE_COUNT) return false;
if (settings->gate_a_mode >= GATE_A_MODE_COUNT) return false;
```

**Refactored implementation:**

```c
// mode_config.h - Add validation limits table
static const uint8_t SETTINGS_LIMITS[] PROGMEM_ATTR = {
    MODE_COUNT,           // mode
    TRIGGER_PULSE_COUNT,  // trigger_pulse_idx
    TRIGGER_EDGE_COUNT,   // trigger_edge
    DIVIDE_DIVISOR_COUNT, // divide_divisor_idx
    CYCLE_TEMPO_COUNT,    // cycle_tempo_idx
    TOGGLE_EDGE_COUNT,    // toggle_edge
    GATE_A_MODE_COUNT,    // gate_a_mode
};
#define SETTINGS_FIELD_COUNT 7

// app_init.c - Loop-based validation
static bool validate_settings_ranges(const AppSettings *settings) {
    const uint8_t *fields = (const uint8_t *)settings;
    for (uint8_t i = 0; i < SETTINGS_FIELD_COUNT; i++) {
        uint8_t limit = PROGMEM_READ_BYTE(&SETTINGS_LIMITS[i]);
        if (fields[i] >= limit) return false;
    }
    return true;
}
```

#### 2.3 LED Feedback Switch Optimization

**Estimated savings: 40-80 bytes**

The `coordinator_get_led_feedback()` function has a 6-case switch for menu pages:

```c
// coordinator.c:376-403 - Current implementation
switch (page) {
    case PAGE_GATE_CV:
        feedback->setting_value = coord->settings->gate_a_mode;
        feedback->setting_max = GATE_A_MODE_COUNT;
        break;
    // ... 5 more cases
}
```

**Refactored implementation using lookup table:**

```c
// mode_config.h - Add settings field offset table
typedef struct {
    uint8_t offset;  // Offset into AppSettings struct
    uint8_t max;     // Maximum value + 1
} PageSettingMap;

static const PageSettingMap PAGE_SETTINGS[] PROGMEM_ATTR = {
    { offsetof(AppSettings, gate_a_mode),       GATE_A_MODE_COUNT },
    { offsetof(AppSettings, trigger_edge),      TRIGGER_EDGE_COUNT },
    { offsetof(AppSettings, trigger_pulse_idx), TRIGGER_PULSE_COUNT },
    { offsetof(AppSettings, toggle_edge),       TOGGLE_EDGE_COUNT },
    { offsetof(AppSettings, divide_divisor_idx),DIVIDE_DIVISOR_COUNT },
    { offsetof(AppSettings, cycle_tempo_idx),   CYCLE_TEMPO_COUNT },
    { 0xFF, 1 },  // PAGE_CV_GLOBAL - not used
    { 0xFF, 1 },  // PAGE_MENU_TIMEOUT - not used
};

// coordinator.c - Table-driven lookup
if (coord->settings && page < PAGE_COUNT) {
    PageSettingMap map;
    PROGMEM_READ_STRUCT(&map, &PAGE_SETTINGS[page], sizeof(map));
    if (map.offset != 0xFF) {
        const uint8_t *base = (const uint8_t *)coord->settings;
        feedback->setting_value = base[map.offset];
        feedback->setting_max = map.max;
    }
}
```

### Phase 3: Data Consolidation

#### 3.1 LED Color Palette Optimization

**Estimated savings: 80-150 bytes**

LED colors are defined multiple times:

1. `mode_handlers.h:84-107` - 15 `#define` constants (no code, but used inline)
2. `led_feedback.c:12-18` - `mode_colors[]` array (15 bytes in RAM/PROGMEM)
3. `led_feedback.c:21-31` - `page_colors[]` array (24 bytes)

**Option A: Derive page colors from mode colors**

Page colors largely correspond to mode colors. Reduce `page_colors[]`:

```c
// led_feedback.c - Use mode colors where applicable
NeopixelColor led_feedback_get_page_color(uint8_t page) {
    // First 6 pages map to mode colors
    static const uint8_t page_to_mode[] PROGMEM_ATTR = {
        MODE_GATE,     // PAGE_GATE_CV
        MODE_TRIGGER,  // PAGE_TRIGGER_BEHAVIOR
        MODE_TRIGGER,  // PAGE_TRIGGER_PULSE_LEN (darker variant)
        MODE_TOGGLE,   // PAGE_TOGGLE_BEHAVIOR
        MODE_DIVIDE,   // PAGE_DIVIDE_DIVISOR
        MODE_CYCLE,    // PAGE_CYCLE_PATTERN
    };

    if (page < 6) {
        uint8_t mode = PROGMEM_READ_BYTE(&page_to_mode[page]);
        return mode_colors[mode];
    }
    // Global pages get special colors
    if (page == PAGE_CV_GLOBAL) return (NeopixelColor){255, 255, 255};
    return (NeopixelColor){128, 128, 128};
}
```

**Option B: Use indexed color palette**

Define a small palette of distinct colors and index into it:

```c
// neopixel.h - Compact color palette
typedef enum {
    COLOR_OFF = 0,
    COLOR_GREEN,    // Gate mode
    COLOR_CYAN,     // Trigger mode
    COLOR_ORANGE,   // Toggle mode
    COLOR_MAGENTA,  // Divide mode
    COLOR_YELLOW,   // Cycle mode
    COLOR_WHITE,    // Global
    COLOR_GRAY,     // Timeout
    COLOR_COUNT
} ColorIndex;

static const NeopixelColor PALETTE[] PROGMEM_ATTR = {
    {0, 0, 0},      // OFF
    {0, 255, 0},    // GREEN
    {0, 128, 255},  // CYAN
    {255, 64, 0},   // ORANGE
    {255, 0, 255},  // MAGENTA
    {255, 255, 0},  // YELLOW
    {255, 255, 255},// WHITE
    {128, 128, 128},// GRAY
};

// Mode/page lookup uses 1-byte indices instead of 3-byte RGB
static const uint8_t MODE_COLORS[] PROGMEM_ATTR = {
    COLOR_GREEN, COLOR_CYAN, COLOR_ORANGE, COLOR_MAGENTA, COLOR_YELLOW
};
```

### Implementation Order

The phases should be implemented in order, with measurements after each change:

```
Phase 1.1: LTO
  ├── Modify CMakeLists.txt
  ├── Full build + size check
  └── Run test suite to verify correctness

Phase 1.2: Conditional compile
  ├── Add NDEBUG guards
  ├── Build + size check
  └── Verify test build still has functions

Phase 2.1: FSM simplification (Option A first)
  ├── Remove on_update from State struct
  ├── Remove fsm_update() function
  ├── Build + size check
  └── Run tests

Phase 2.2: Settings validation refactor
  ├── Add SETTINGS_LIMITS table
  ├── Replace if-chain with loop
  └── Build + test

Phase 2.3: LED feedback optimization
  ├── Add PAGE_SETTINGS table
  ├── Replace switch with lookup
  └── Build + test

Phase 3.1: Color palette (if needed)
  ├── Choose Option A or B based on savings needed
  └── Implement + test
```

### Interface Changes

#### New types in mode_config.h:

```c
// Settings validation table entry (Phase 2.2)
// No new type needed - uses existing uint8_t array

// Page-to-setting mapping (Phase 2.3)
typedef struct {
    uint8_t offset;
    uint8_t max;
} PageSettingMap;
```

#### Modified function signatures:

None - all changes are internal implementation details.

#### Removed functions:

```c
// fsm.c - Remove (Phase 2.1)
void fsm_update(FSM *fsm);  // Never called

// hal.c - Conditionally removed (Phase 1.2)
#ifndef NDEBUG
void hal_advance_time(uint32_t ms);
void hal_reset_time(void);
#endif
```

### Data Structures

#### Modified State struct (Phase 2.1, Option A):

```c
// Before
typedef struct {
    uint8_t id;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_update)(void);  // Remove this
} State;

// After
typedef struct {
    uint8_t id;
    void (*on_enter)(void);
    void (*on_exit)(void);
} State;
```

#### New PROGMEM tables:

```c
// Phase 2.2 - Settings validation limits
static const uint8_t SETTINGS_LIMITS[] PROGMEM_ATTR;

// Phase 2.3 - Page-to-setting mapping
static const PageSettingMap PAGE_SETTINGS[] PROGMEM_ATTR;

// Phase 3.1 Option B - Color palette
static const NeopixelColor PALETTE[] PROGMEM_ATTR;
static const uint8_t MODE_COLORS[] PROGMEM_ATTR;
```

### Error Handling

| Change | Error Handling |
|--------|----------------|
| LTO | If build fails, disable and investigate |
| Conditional compile | TEST_BUILD flag preserves test functionality |
| FSM simplification | Existing tests validate behavior unchanged |
| Validation refactor | Same validation logic, different implementation |
| Color lookup | Bounds checking on page index |

### Testing Strategy

**Phase 1 (Build changes):**
- Full test suite must pass after each change
- Manual hardware testing of all modes
- Size comparison documented in commit message

**Phase 2 (Code changes):**
- Unit tests for affected modules
- Add test cases for edge conditions
- Regression test: all existing tests pass

**Phase 3 (Data changes):**
- Visual verification of all LED colors
- Test menu navigation color transitions
- Verify mode indicator colors match spec

**Size verification script:**

```bash
#!/bin/bash
# scripts/size_compare.sh - Track optimization progress
echo "=== Size Optimization Tracking ==="
echo "Baseline: [record initial size]"
avr-size --format=avr --mcu=attiny85 build/firmware/gatekeeper
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `CMakeLists.txt` | Modify | Add -flto flag, add -DNDEBUG |
| `include/hardware/hal.h` | Modify | Add NDEBUG guards around test functions |
| `src/hardware/hal.c` | Modify | Add NDEBUG guards around test function implementations |
| `include/fsm/fsm.h` | Modify | Remove on_update from State struct |
| `src/fsm/fsm.c` | Modify | Remove fsm_update() function |
| `include/config/mode_config.h` | Modify | Add SETTINGS_LIMITS table, PageSettingMap type |
| `src/app_init.c` | Modify | Refactor validation to use loop |
| `src/core/coordinator.c` | Modify | Refactor LED feedback switch to table lookup |
| `src/output/led_feedback.c` | Modify | Consolidate color tables (Phase 3) |
| `scripts/size_compare.sh` | Create | Track size changes across phases |

## Dependencies

- AVR-GCC with LTO support (standard in recent avr-gcc)
- No external library changes

## Alternatives Considered

### 1. Rewrite in Assembly

Hand-optimized assembly for critical paths (neopixel, FSM).

**Rejected:** High effort, maintenance burden, marginal gains over LTO.

### 2. Reduce Feature Set

Remove modes or menu pages to save space.

**Rejected:** Features are core to product value. Optimization should preserve functionality.

### 3. Use Smaller Data Types Everywhere

Replace uint16_t with uint8_t where possible.

**Rejected:** Already using appropriate types. Risk of overflow bugs outweighs small savings.

### 4. External Flash/EEPROM for Data

Store lookup tables in EEPROM to free flash.

**Rejected:** Adds complexity, slower access, EEPROM is limited (512 bytes).

### 5. Different MCU

Upgrade to ATtiny85V (same footprint, more flash).

**Rejected:** ATtiny85 is the largest in the family. Next step up changes footprint/cost.

## Open Questions

1. **LTO compatibility:** Does LTO work correctly with the inline assembly in `neopixel.c`? Need to verify timing-critical code still functions.

2. **FSM simplification depth:** How far should we go? Option A (remove on_update) is safe. Options B/C save more but require more refactoring.

3. **Color palette approach:** Is the visual difference acceptable if we reduce color precision for larger savings?

4. **Future feature budget:** How much headroom is needed? Target 85% leaves ~1200 bytes. Is this sufficient?

## Implementation Checklist

### Phase 1: Build System
- [x] Add `-flto` to CMAKE_C_FLAGS and CMAKE_EXE_LINKER_FLAGS
- [x] Run full test suite
- [x] Document size savings (844 bytes)
- [~] Add `-DNDEBUG` guards - SKIPPED (functions don't exist in prod HAL)

### Phase 2: Code Simplification
- [x] Add SETTINGS_LIMITS table to app_init.c
- [x] Refactor validate_settings_ranges() to use loop
- [x] Run full test suite
- [x] Document size savings (28 bytes)
- [~] FSM simplification - SKIPPED (preserving callback infrastructure)
- [~] LED feedback switch refactor - SKIPPED (readability priority)

### Phase 3: Data Consolidation
- [~] Color palette optimization - SKIPPED (readability priority for reference project)

### Final Verification
- [x] Full regression test on hardware
- [x] Update CLAUDE.md with final sizes
- [x] Update README.md with final sizes
