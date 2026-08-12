---
type: ap
id: AP-005
status: active
created: 2025-12-21
modified: 2025-12-21
supersedes: null
superseded_by: null
obsoleted_by: null
related: [RPT-001, ADR-004]
---

# AP-005: Fix Core Signal Processing

## Status

Active

## Created

2025-12-21

## Goal

Fix the two P1 issues from hardware testing (RPT-001):
1. CV input not triggering gate output
2. Gate output staying HIGH when in menu mode

After this work, the gate output should respond to CV input in all modes, and signal processing should continue (CV only, no buttons) while in menu mode.

## Context

During breadboard prototype testing, two critical issues were discovered:

**Issue 1: CV Input Not Working**
- CV input (PB3/ADC3) does not trigger any gate output response
- Only Button B controls the output
- ADC + software hysteresis implemented per ADR-004, but signal not reaching mode handlers

**Issue 2: Menu Mode Breaks Output**
- When entering menu, gate output goes HIGH and stays HIGH
- Expected: CV should continue controlling output, only button actions suppressed
- Currently: Mode handler processing appears to be skipped entirely in menu mode

## Tasks

### Phase 1: Diagnose CV Input Path

- [ ] **1.1** Verify ADC hardware configuration in `hal.c`
  - Check ADMUX register setup (channel 3, left-adjust)
  - Check ADCSRA register (enable, prescaler)
  - Verify `adc_read()` implementation

- [ ] **1.2** Test CV input module in isolation
  - Check `cv_input_update()` hysteresis logic
  - Verify thresholds: HIGH=128 (2.5V), LOW=77 (1.5V)
  - Add simulator test or debug output

- [ ] **1.3** Trace CV path through coordinator
  - In `coordinator_update()`: verify `cv_adc = p_hal->adc_read(CV_ADC_CHANNEL)`
  - Verify `cv_state = cv_input_update(&coord->cv_input, cv_adc)`
  - Check if `cv_state` is being used in mode handler input

- [ ] **1.4** Verify mode handler receives CV input
  - In Gate mode: `input_state` should include CV, not just button
  - Check: `input_state = event_processor_b_pressed(&coord->events)` - this only checks button!
  - **FIX NEEDED**: OR cv_state with button state for mode handler input

### Phase 2: Fix CV Input Integration

- [ ] **2.1** Modify `coordinator_update()` to include CV in mode handler input
  ```c
  // Current (broken):
  bool input_state = event_processor_b_pressed(&coord->events);

  // Fixed:
  bool input_state = event_processor_b_pressed(&coord->events) || cv_state;
  ```

- [ ] **2.2** Update Gate mode A button handling to also consider CV
  - Current code only ORs button A when `gate_a_mode == GATE_A_MODE_MANUAL`
  - This is correct - A button is alternate function, not CV

- [ ] **2.3** Test CV input on hardware
  - Apply 5V signal to CV input
  - Verify gate output responds
  - Test with different modes

### Phase 3: Fix Menu Mode Signal Processing

- [ ] **3.1** Analyze current menu mode behavior
  - In `coordinator_update()`, find where mode handler is called
  - Check condition: `if (fsm_get_state(&coord->top_fsm) == TOP_PERFORM)`
  - **PROBLEM**: Mode handler only runs in PERFORM state, not MENU

- [ ] **3.2** Modify coordinator to process CV in menu mode
  ```c
  // Move mode handler call outside of PERFORM-only block
  // But only use CV input, not button input, when in menu

  ModeState mode = (ModeState)fsm_get_state(&coord->mode_fsm);
  bool input_state;

  if (fsm_get_state(&coord->top_fsm) == TOP_PERFORM) {
      // In perform mode: CV OR button
      input_state = cv_state || event_processor_b_pressed(&coord->events);
      // Also handle button A alternate functions here
  } else {
      // In menu mode: CV only (buttons used for menu navigation)
      input_state = cv_state;
  }

  mode_handler_process(mode, &coord->mode_ctx, input_state, &coord->output_state);
  ```

- [ ] **3.3** Test menu mode on hardware
  - Enter menu mode
  - Apply CV signal
  - Verify gate output responds to CV
  - Verify buttons don't affect output (used for menu nav)

### Phase 4: Verification

- [ ] **4.1** Run unit tests
  - Ensure existing tests pass
  - Add tests for CV input path if missing

- [ ] **4.2** Run simulator tests
  - Test CV input triggering in all modes
  - Test menu mode CV passthrough

- [ ] **4.3** Hardware verification
  - Test all 5 modes with CV input
  - Test menu entry/exit with CV signal present
  - Verify no regression in button functionality

## Notes

**Key insight from code review:**
The root cause of Issue 1 is likely in `coordinator_update()` around line 280:
```c
bool input_state = event_processor_b_pressed(&coord->events);
```
This only checks the button state, not the CV state. The `cv_state` variable is computed but only used for event generation (EVT_CV_RISE/FALL), not for direct mode handler input.

**Key insight for Issue 2:**
The mode handler call is wrapped in:
```c
if (fsm_get_state(&coord->top_fsm) == TOP_PERFORM) {
    // ... mode_handler_process() called here
}
```
This means no signal processing happens at all in menu mode.

## Completion Criteria

- [ ] CV input triggers gate output in Gate mode
- [ ] CV input triggers appropriate response in all 5 modes
- [ ] Menu mode: CV continues to control output
- [ ] Menu mode: Button B does NOT affect output (used for menu)
- [ ] All unit tests pass
- [ ] Hardware verified on breadboard prototype

## References

- [RPT-001: Hardware Testing Findings](../reports/RPT-001-hardware-testing-findings.md)
- [ADR-004: Analog CV Input with Software Hysteresis](../decision-records/004-analog-cv-input.md)
- [Flowchart: gk-fw-flow-2025-12-10.png](../../resources/images/gk-fw-flow-2025-12-10.png)
- `src/core/coordinator.c` - Main signal routing
- `src/input/cv_input.c` - CV hysteresis logic

---

## Addenda
