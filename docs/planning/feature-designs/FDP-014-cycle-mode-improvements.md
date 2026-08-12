---
type: fdp
id: FDP-014
status: proposed
created: 2025-12-21
modified: 2025-12-21
supersedes: null
superseded_by: null
obsoleted_by: null
related: [RPT-001]
---

# FDP-014: Cycle Mode Improvements

## Status

Proposed

## Summary

Refactor Cycle mode to support multiple tempo sources (tap tempo, tempo learn, fixed tempo) and optional CV control. This replaces the current single-page tempo selection with a more flexible two-page system plus optional CV modulation.

## Motivation

The current Cycle mode has a single menu page that directly selects BPM values. Per the flowchart and user requirements:

1. **Tap Tempo**: User taps button to set tempo manually
2. **Tempo Learn**: Module measures incoming clock to auto-detect tempo
3. **Fixed Tempo**: Select from preset BPM values (current behavior)

Additionally, CV control of tempo would enable:
- Musical pitch tracking (V/Oct) - Cycle acts like a clock VCO
- Linear frequency modulation
- External clock sync

## Detailed Design

### Overview

Replace the single `PAGE_CYCLE_PATTERN` with two pages:

1. **PAGE_CYCLE_A_BEHAVIOR** - Tempo source selection
2. **PAGE_CYCLE_FIXED_TEMPO** - Fixed tempo BPM selection (only relevant when source = Fixed)

Add optional third page for CV control mode.

### Menu Page Structure

#### PAGE_CYCLE_A_BEHAVIOR (Tempo Source)

| Index | Setting | Description |
|-------|---------|-------------|
| 0 | Tap Tempo | A:tap sets tempo from tap interval |
| 1 | Tempo Learn | Measures CV/button input to detect tempo |
| 2 | Fixed Tempo | Use preset BPM from PAGE_CYCLE_FIXED_TEMPO |

**LED Feedback:**
- LED X: Blue (Cycle mode color), blink (first page)
- LED Y: off/on/blink for value 0/1/2

#### PAGE_CYCLE_FIXED_TEMPO (BPM Selection)

| Index | BPM | Period (ms) |
|-------|-----|-------------|
| 0 | 60 | 1000 |
| 1 | 90 | 667 |
| 2 | 120 | 500 |
| 3 | 160 | 375 |

**LED Feedback:**
- LED X: Blue, glow (second page of Cycle group)
- LED Y: off/on/blink/glow for value 0/1/2/3

#### PAGE_CYCLE_CV_MODE (Optional - CV Control)

| Index | Mode | Description |
|-------|------|-------------|
| 0 | Off | CV ignored, internal tempo only |
| 1 | V/Oct | Exponential (1V = double frequency) |
| 2 | Linear | Voltage = frequency multiplier |

### Tap Tempo Implementation

When `cycle_a_mode == TAP_TEMPO`:
- Track timestamp of A:tap events
- Calculate interval between consecutive taps
- Apply simple averaging (last 2-4 taps)
- Set internal period from averaged interval
- Store reasonable default until first tap (1000ms = 60 BPM)

### Tempo Learn Implementation

When `cycle_a_mode == TEMPO_LEARN`:
- Measure time between rising edges on CV input
- Apply averaging/filtering for stability
- Auto-update internal period
- Fall back to default if no edges detected within timeout

### CV Control Implementation (V/Oct)

When `cycle_cv_mode == VOCT`:
- Read CV voltage via ADC
- Calculate frequency multiplier: `mult = 2^(voltage)`
- At 0V: base frequency (from tap/learn/fixed)
- At 1V: 2x frequency (1 octave up)
- At 2V: 4x frequency (2 octaves up)
- Negative voltages (if supported): slower

### Data Structures

```c
// New settings fields (in AppSettings)
uint8_t cycle_a_mode;        // 0=tap, 1=learn, 2=fixed
uint8_t cycle_fixed_idx;     // Index into fixed BPM table (0-3)
uint8_t cycle_cv_mode;       // 0=off, 1=voct, 2=linear (optional)

// Cycle mode context additions
typedef struct {
    uint32_t last_tap_time;      // For tap tempo
    uint32_t tap_interval;       // Measured tap interval
    uint32_t learn_last_edge;    // For tempo learn
    uint32_t learned_period;     // Detected period
    // ... existing fields
} CycleModeContext;
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/core/states.h` | Modify | Rename PAGE_CYCLE_PATTERN to PAGE_CYCLE_A_BEHAVIOR, add PAGE_CYCLE_FIXED_TEMPO |
| `include/config/mode_config.h` | Modify | Add CYCLE_A_MODE_* defines, update tempo tables |
| `include/app_init.h` | Modify | Add new settings fields to AppSettings |
| `src/app_init.c` | Modify | Update defaults, validation limits |
| `src/modes/mode_handlers.c` | Modify | Implement tap tempo, tempo learn logic |
| `src/core/coordinator.c` | Modify | Update menu page handling, A:tap routing |
| `src/output/led_feedback.c` | Modify | Add page color/animation for new page |

## Implementation Phases

### Phase 1: Menu Structure

- Add PAGE_CYCLE_FIXED_TEMPO page
- Rename PAGE_CYCLE_PATTERN to PAGE_CYCLE_A_BEHAVIOR
- Update settings struct with `cycle_a_mode` and `cycle_fixed_idx`
- Update LED feedback for two Cycle pages
- Fixed tempo mode works (existing behavior, just split into two pages)

### Phase 2: Tap Tempo

- Implement A:tap detection in Cycle mode during Perform
- Calculate and apply tap interval
- Store reasonable default until first tap

### Phase 3: Tempo Learn

- Implement edge detection on CV input
- Calculate period from edge timing
- Auto-update cycle period

### Phase 4: CV Control (Optional)

- Add PAGE_CYCLE_CV_MODE
- Implement V/Oct calculation
- Implement linear mode
- Test with CV sources

## Alternatives Considered

### Single Combined Page

Keep one page but cycle through: Tap → Learn → 60 → 90 → 120 → 160 BPM.

**Rejected**: Confusing UX - mixes mode selection with value selection.

### Three Separate Settings

Have three separate indexed settings (source, fixed BPM, CV mode) without menu pages.

**Rejected**: No way to configure without dedicated pages.

### CV as Sync Input Only

Instead of V/Oct or linear CV, just sync to incoming clock.

**Considered for Phase 3**: Could be simpler than V/Oct. May add as option 3 in CV mode.

## Open Questions

1. **Tap Tempo Averaging**: How many taps to average? 2-4 seems reasonable.

2. **Tempo Learn Timeout**: How long to wait for edges before falling back to default? 5 seconds?

3. **V/Oct Range**: What voltage range to support? 0-3V gives 8x frequency range.

4. **A Button Conflict**: In Tap Tempo mode, A:tap sets tempo. Does this conflict with menu entry gesture (A:hold → B:hold)? Should be fine since tap vs hold are distinct.

5. **EEPROM Space**: Adding 2-3 new bytes to AppSettings. Current struct is 8 bytes with 1 reserved. May need to expand or use reserved byte.

6. **Flash Budget**: Currently at 86%. New features may push close to limit. Consider which phases are must-have vs nice-to-have.

## References

- [RPT-001: Hardware Testing Findings](../reports/RPT-001-hardware-testing-findings.md) - Finding 5 (alternate functions)
- [Flowchart: gk-fw-flow-2025-12-10.png](../../resources/images/gk-fw-flow-2025-12-10.png) - Cycle Mode A behavior spec
- [V/Oct Standard](https://en.wikipedia.org/wiki/CV/gate#CV) - Eurorack pitch CV standard

---

## Addenda

