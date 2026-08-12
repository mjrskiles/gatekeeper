---
type: report
id: RPT-001
status: draft
created: 2025-12-21
modified: '2025-12-21'
supersedes: null
superseded_by: null
obsoleted_by: null
related:
- FDP-013
- AP-005
---

# RPT-001: Hardware Testing Findings

## Status

Draft

## Summary

This report documents issues and observations from breadboard prototype testing of the Gatekeeper firmware on ATtiny85 hardware. Testing was conducted on 2025-12-21 after successful hardware bringup (8MHz clock, active-low buttons, RGB neopixel order).

Reference flowchart: `docs/resources/images/gk-fw-flow-2025-12-10.png`

## Findings

### Finding 1: CV Input Not Triggering Gate Output

**Severity:** High (core functionality broken)

**Observed:** CV input (PB3/ADC) does not trigger any response on the gate output. Only Button B controls the output.

**Expected:** Per flowchart, CV IN should trigger gate output in all modes. The CV input path through ADC with software hysteresis (ADR-004) should produce rising/falling edges that feed into the mode handlers.

**Likely Cause:** Need to investigate `coordinator_update()` - the CV input may not be properly routed to the mode handler input, or the ADC/hysteresis logic may have an issue.

### Finding 2: Gate Output Stays High When Entering Menu

**Severity:** High (incorrect behavior)

**Observed:** When entering menu mode (at least in Gate mode), the gate output goes HIGH and stays HIGH for the entire duration of menu mode.

**Expected:** Per flowchart, while in menu:
- CV IN should continue to control gate output (signal processing continues)
- Button presses should be ignored for signal processing (used for menu navigation instead)

**Likely Cause:** In `coordinator_update()`, mode handler processing may be skipped entirely when in menu state, or the output state is being set incorrectly on menu entry.

### Finding 3: LED Y (Activity) Shows White in Menu

**Severity:** Medium (UI feedback incorrect)

**Observed:** When in menu mode, LED Y turns white and stays white regardless of the current page or selected value.

**Expected:** Per flowchart, LED Y should indicate the current selection value:
- 0: LED Y OFF
- 1: LED Y ON
- 2: LED Y BLINKING
- 3: LED Y GLOWING

The current value for each menu page setting should be displayed via LED Y brightness/animation state.

**Likely Cause:** `led_feedback_update()` menu logic sets a static white color for the activity LED instead of using the setting value to determine brightness/animation.

### Finding 4: Menu Exit Gesture Should Be A:hold

**Severity:** Low (UX improvement)

**Observed:** Menu is entered with `A:hold→B:hold` and exited with the same gesture.

**Requested:** User wants menu exit to be simply `A:hold` (single button hold) for easier operation.

**Impact:** Requires changes to:
- Event processor or coordinator to detect A:hold as menu exit
- Transition table for Top FSM

### Finding 5: Alternate Functions (Button A) Not Implemented

**Severity:** Medium (features not implemented)

**Observed:** Button A has no effect in Perform mode across all modes.

**Expected:** Per flowchart, each mode has configurable Button A behavior set via menu:

| Mode | A Button Options |
|------|------------------|
| Gate | 0: A:tap toggles invert CV IN<br>1: A:tap toggles CV IN ignore<br>2: A:any momentarily inverts CV IN |
| Trigger | 0: A:tap toggles rising/falling edge<br>1: A:tap toggles CV IN ignore |
| Toggle | 0: A:tap toggles rising/falling edge<br>1: A:tap toggles CV IN ignore<br>2: A:any momentarily inverts CV IN |
| Divide | (not specified in flowchart) |
| Cycle | 0: Tap Tempo<br>1: Tempo Learn |

**Current State:** The menu pages exist and values can be cycled, but the actual functionality is not implemented in the mode handlers.

### Finding 6: Buffered LED Redundancy

**Severity:** Informational (hardware consideration)

**Observed:** LED Y (activity neopixel) duplicates the behavior of the buffered LED connected directly to the gate output circuit. LED Y shows the same on/off state but with mode-specific color.

**Consideration:** User is considering removing the buffered LED from the hardware design since LED Y provides the same information plus color coding.

**Impact:** No firmware changes needed. Hardware design decision only.

### Finding 7: Additional Menu Pages from Flowchart

**Severity:** Informational (documentation)

The flowchart shows menu pages not fully documented elsewhere:

| Page | Setting | Values |
|------|---------|--------|
| Gate Mode | A button behavior | 0-2 (see Finding 5) |
| Trigger Mode (A) | A button behavior | 0-1 |
| Trigger Mode (pulse) | Pulse length | 0:10ms, 1:20ms, 2:50ms, 3:1ms |
| Toggle Mode | A button behavior | 0-2 |
| Clock Divider | Divisor | 0:/2, 1:/4, 2:/8, 3:/24 |
| Cycle Mode A | A behavior | 0:Tap Tempo, 1:Tempo Learn |
| CV IN Global | CV behavior | 0:Normal, 1:Ignore CV |
| System Reset | Reset option | 0:Normal, 1:Reset defaults on menu exit |

## Data/Evidence

### Test Environment
- MCU: ATtiny85 @ 8MHz (LFUSE=0xE2)
- Firmware: v0.8.0 with LTO (6854 bytes, 83.7% flash)
- Neopixels: 2x WS2812B (RGB order, not GRB)
- Buttons: Active-low with internal pull-ups
- Programmer: Olimex AVR-ISP500 (stk500v2)

### Verified Working
- Mode LED (X) shows correct colors for all 5 modes
- Activity LED (Y) shows correct mode color when gate is active in Perform mode
- Mode cycling via B:hold→A:hold gesture works
- Menu entry via A:hold→B:hold gesture works
- Menu page navigation (A:tap) works
- Menu value cycling (B:tap) works
- Factory reset detection works
- EEPROM save/load works
- Watchdog timer enabled and stable

### Not Working / Incomplete
- CV input to gate output path
- Menu mode signal processing (output stuck high)
- LED Y menu feedback (shows white instead of value)
- Button A alternate functions in all modes
- Tap tempo / tempo learn for Cycle mode
- CV IN global ignore setting
- System reset menu option

## Recommendations

### Priority 1: Core Functionality
1. **Fix CV input path** - Debug ADC reading and hysteresis in `cv_input.c`, verify routing through `coordinator_update()` to mode handlers
2. **Fix menu mode output** - Modify coordinator to continue processing CV input through mode handlers while in menu, only suppress button-triggered actions

### Priority 2: UI/UX
3. **Fix LED Y menu feedback** - Update `led_feedback_update()` to show setting value via LED Y state (off/on/blink/glow)
4. **Implement A:hold menu exit** - Add new transition or modify event processing

### Priority 3: Features
5. **Implement alternate functions** - Add Button A handling to each mode handler based on settings:
   - Gate: CV invert, CV ignore, momentary invert
   - Trigger: Edge toggle, CV ignore
   - Toggle: Edge toggle, CV ignore, momentary invert
   - Cycle: Tap tempo, tempo learn
6. **Implement global CV ignore** - Add setting check in coordinator before passing CV to mode handlers
7. **Implement system reset option** - Add menu page action to reset EEPROM on menu exit

### Priority 4: Hardware Decision
8. **Evaluate buffered LED removal** - No firmware impact, but document decision if hardware changes

## Appendices

### A. Flowchart Reference

See `docs/resources/images/gk-fw-flow-2025-12-10.png` for the complete firmware flow diagram showing:
- Init sequence (left, white)
- Perform mode states (center, blue)
- Menu states and pages (right, yellow/green)

### B. Current Code Locations

| Component | File | Function |
|-----------|------|----------|
| CV Input | `src/input/cv_input.c` | `cv_input_update()` |
| Coordinator | `src/core/coordinator.c` | `coordinator_update()` |
| Mode Handlers | `src/modes/mode_handlers.c` | `mode_handler_process()` |
| LED Feedback | `src/output/led_feedback.c` | `led_feedback_update()` |
| Event Processing | `src/events/events.c` | `event_processor_update()` |

## References

- [ADR-004: Analog CV Input with Software Hysteresis](../decision-records/004-analog-cv-input.md)
- [FDP-013: Firmware Size Optimization](../feature-designs/FDP-013-firmware-size-optimization.md)
- [Flowchart: gk-fw-flow-2025-12-10.png](../../resources/images/gk-fw-flow-2025-12-10.png)
- [CLAUDE.md: Project Context](../../../CLAUDE.md)

---

## Addenda
