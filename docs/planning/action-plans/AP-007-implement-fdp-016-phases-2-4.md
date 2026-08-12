---
type: ap
id: AP-007
status: active
created: 2025-12-23
modified: '2025-12-23'
supersedes: null
superseded_by: null
obsoleted_by: null
related: [FDP-016]
---

# AP-007: Implement FDP-016 Phases 2-4

## Status

Active

## Created

2025-12-23

## Goal

Complete the remaining phases of FDP-016 (Simulator Fidelity and Fault Injection):
- Phase 2: Integration test suite for CV signal chains
- Phase 3: Extended fault injection (EEPROM, timer)
- Phase 4: Documentation

## Context

Phase 1 (ADC Fault Injection MVP) was completed on 2025-12-23:
- Added `SimAdcMode` enum with 5 fault modes (normal, timeout, stuck_low, stuck_high, noisy)
- Implemented `sim_adc_set_mode()` and updated `sim_adc_read()`
- Added `fault_adc` socket command handler
- Extended script language with `fault` and `cv` actions
- Created `adc_timeout_recovery.gks` test script - all tests pass

The remaining work builds on this foundation to improve test coverage and add more fault injection capabilities.

## Tasks

### Phase 2: Integration Test Suite

Create test scripts that verify complete signal chains through the system.

- [x] **2.1** Create `cv_gate_chain.gks` - Gate mode CV signal chain test
  - CV input at various levels → hysteresis → mode handler → output
  - Test threshold boundaries (127, 128, 129 for high; 76, 77, 78 for low)
  - Verify output tracks input in gate mode

- [x] **2.2** Create `cv_trigger_chain.gks` - Trigger mode signal chain test
  - CV rising edge → trigger pulse generation
  - Verify pulse duration (default 10ms)
  - Test rapid retriggering behavior

- [x] **2.3** Create `cv_toggle_chain.gks` - Toggle mode signal chain test
  - CV rising edge → output state flip
  - Multiple toggles in sequence
  - Verify state persistence

- [x] **2.4** Create `cv_menu_passthrough.gks` - Menu mode CV passthrough test
  - Enter menu mode (hold A, then hold B)
  - Verify CV still controls output
  - Verify button B does NOT affect output (used for menu nav)
  - Exit menu and verify normal operation resumes

- [x] **2.5** Create `hysteresis_bounds.gks` - Threshold boundary test
  - Test exact boundary values per ADR-004
  - Verify no chatter at threshold crossings
  - Test with noisy ADC fault mode

- [x] **2.6** Add CI integration
  - Create script to run all .gks tests in batch mode
  - Return non-zero exit code if any test fails
  - Document in README.md

### Phase 3: Extended Fault Injection

Add fault injection for EEPROM and timer subsystems.

- [x] **3.1** Add EEPROM fault injection to sim_hal
  ```c
  typedef enum {
      SIM_EEPROM_NORMAL,
      SIM_EEPROM_WRITE_FAIL,    // Writes silently fail
      SIM_EEPROM_READ_FF,       // All reads return 0xFF
      SIM_EEPROM_CORRUPT,       // Random bit flips on read
  } SimEepromMode;

  void sim_eeprom_set_mode(SimEepromMode mode);
  SimEepromMode sim_eeprom_get_mode(void);
  ```

- [x] **3.2** Add socket command for EEPROM fault
  ```json
  {"cmd": "fault_eeprom", "mode": "write_fail"}
  {"cmd": "fault_eeprom", "mode": "normal"}
  ```

- [x] **3.3** Add script action for EEPROM fault
  - `fault eeprom <mode>`

- [x] **3.4** Create `eeprom_fault_modes.gks` test
  - Test settings save with write_fail mode
  - Verify system continues operating with faults
  - Verify recovery when faults cleared

- [ ] **3.5** Add timer drift simulation (optional, low priority)
  ```c
  void sim_timer_set_drift(int32_t ppm);  // Simulate clock drift
  ```
  - Useful for testing timing-sensitive modes (Cycle, Trigger pulse)

### Phase 4: Documentation

- [x] **4.1** Create `docs/SIMULATOR.md`
  - Overview of simulator architecture
  - How to run: interactive, batch, JSON, socket modes
  - Script language reference (all actions, targets, syntax)
  - Fault injection guide with examples
  - Socket protocol reference

- [x] **4.2** Document behavioral guarantees
  - What is identical between sim and hardware
  - Known differences (timing precision, ISR simulation)
  - When to use sim vs hardware testing

- [x] **4.3** Update CLAUDE.md
  - Reference new SIMULATOR.md
  - Note new script actions (fault, cv)
  - Update test count if changed

- [x] **4.4** Update README.md
  - Add fault injection to simulator section
  - Add CI test running instructions

## Notes

**Script language extensions implemented in Phase 1:**
- `fault adc <mode>` - Set ADC fault mode
- `cv <value>` - Set CV to specific 0-255 value

**Socket commands implemented in Phase 1:**
- `{"cmd": "fault_adc", "mode": "<mode>", "amplitude": <n>}`

**Prioritization:**
- Phase 2 provides immediate value for regression testing
- Phase 3 EEPROM faults are useful but less critical
- Phase 3 timer drift is optional/low priority
- Phase 4 documentation should accompany each phase

## Completion Criteria

- [x] All Phase 2 test scripts created and passing
- [x] CI script runs all tests with single command
- [x] EEPROM fault injection working (Phase 3)
- [ ] SIMULATOR.md created with complete reference (Phase 4)
- [x] All builds pass (firmware, tests, simulator)

## References

- [FDP-016: Simulator Fidelity and Fault Injection](../feature-designs/FDP-016-simulator-fidelity-and-fault-injection.md)
- [ADR-004: Analog CV Input with Software Hysteresis](../decision-records/004-analog-cv-input.md)
- [RPT-001: Hardware Testing Findings](../reports/RPT-001-hardware-testing-findings.md)
- `sim/sim_hal.c` - Simulator HAL with fault injection
- `sim/scripts/adc_timeout_recovery.gks` - Example fault injection test

---

## Addenda
