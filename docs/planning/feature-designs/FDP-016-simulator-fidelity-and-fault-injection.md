---
type: fdp
id: FDP-016
status: in progress
created: 2025-12-22
modified: '2025-12-23'
supersedes: null
superseded_by: null
obsoleted_by: null
related:
- AP-005
- ADR-004
- RPT-001
- AP-007
---

# FDP-016: Simulator Fidelity and Fault Injection

## Status

In Progress

## Summary

Enhance the native simulator to better model hardware failure modes and improve test coverage for critical signal paths. This includes fault injection APIs, integration tests for complete signal chains, and documentation of simulator/hardware behavioral guarantees.

## Motivation

During hardware bringup (RPT-001), bugs were discovered that weren't caught by the simulator. While analysis confirmed the simulator architecture is sound (no shortcuts or parallel implementations), several gaps were identified:

1. **No fault injection**: The simulator always returns "happy path" values. Hardware can timeout, return garbage, or exhibit transient failures that the sim cannot model.

2. **ADC timeout not simulated**: Real `hal_adc_read()` returns 128 (mid-scale) on conversion timeout. The simulator always returns the actual CV value, potentially masking code that doesn't handle ADC failures gracefully.

3. **Missing integration tests**: Unit tests verify individual modules, but there are no scripted tests that verify complete signal chains (CV input → hysteresis → coordinator → mode handler → output).

4. **Testing methodology gaps**: Hardware bugs may exist in sim too, but different testing patterns (manual vs scripted, waveforms vs static values) can lead to different code paths being exercised.

### Current Architecture Assessment

| Aspect | Status | Notes |
|--------|--------|-------|
| HAL abstraction | Excellent | Clean interface, proper function pointers |
| Code reuse | Excellent | Same `coordinator.c` runs in all environments |
| JSON output | Good | Decoupled via renderer pattern |
| Socket server | Good | Optional, non-blocking NDJSON protocol |
| Test mocks | Good | Same pattern as sim_hal |

The simulator is architecturally sound. This FDP focuses on enhancing fidelity and test coverage, not fixing fundamental issues.

## Detailed Design

### Overview

Add fault injection capabilities to the simulator HAL, create integration test scripts that verify signal chains, and improve documentation of behavioral guarantees between simulator and hardware.

### Components

#### 1. Fault Injection API

New functions in `sim_hal.c` to inject hardware failure modes:

```c
// ADC fault injection
typedef enum {
    SIM_ADC_NORMAL,      // Return actual CV value
    SIM_ADC_TIMEOUT,     // Always return 128 (timeout value)
    SIM_ADC_STUCK_LOW,   // Always return 0
    SIM_ADC_STUCK_HIGH,  // Always return 255
    SIM_ADC_NOISY,       // Add random noise to readings
} SimAdcMode;

void sim_adc_set_mode(SimAdcMode mode);
void sim_adc_set_noise_amplitude(uint8_t amplitude);  // For NOISY mode

// Timer fault injection
void sim_timer_set_drift(int32_t ppm);  // Simulate clock drift
void sim_timer_inject_stall(uint32_t duration_ms);  // Timer stops advancing

// EEPROM fault injection
void sim_eeprom_set_write_fail(bool enabled);  // Writes silently fail
void sim_eeprom_corrupt_byte(uint16_t addr);   // Flip random bit
```

#### 2. Integration Test Scripts

New test scripts in `sim/scripts/` that verify complete signal chains:

| Script | Purpose |
|--------|---------|
| `cv_gate_chain.gks` | CV input → Gate mode → output |
| `cv_trigger_chain.gks` | CV input → Trigger mode → pulse output |
| `cv_menu_passthrough.gks` | CV continues working while in menu |
| `adc_timeout_recovery.gks` | System recovers from ADC timeout |
| `hysteresis_bounds.gks` | Verify threshold behavior at boundaries |

#### 3. CI Integration

Add simulator integration tests to the build/test workflow:

```bash
# Run all integration tests
./build/sim/sim/gatekeeper-sim --script sim/scripts/cv_gate_chain.gks --batch
```

Exit code 0 = pass, non-zero = assertion failure.

#### 4. Behavioral Documentation

Create `docs/SIMULATOR.md` documenting:
- Simulator vs hardware behavioral guarantees
- Known differences (timing precision, ISR simulation)
- Fault injection usage guide
- Integration test authoring guide

#### 5. Script Language Revision

The current script language (`input_source.c`) has limitations that make comprehensive testing difficult. Proposed improvements:

**Current Language:**
```
<delay_ms> <action> [target] [value]
@<abs_ms> <action> [target] [value]
```

Actions: `press`, `release`, `assert`, `log`, `quit`
Targets: `a`, `b`, `cv`, `output`

**Limitations:**
| Issue | Current Behavior | Problem |
|-------|------------------|---------|
| CV is binary only | `press cv` = 255, `release cv` = 0 | Can't test hysteresis boundaries |
| Limited assert targets | Only `output` | Can't verify `mode`, `page`, `cv_digital` |
| No fault injection | N/A | Can't test failure modes |
| No tap shorthand | Manual press+delay+release | Verbose scripts |
| No hold shorthand | Manual timing | Error-prone |
| No CV waveforms | Static values only | Can't test with LFO/ramp |

**Proposed Extensions:**

| New Action | Syntax | Description |
|------------|--------|-------------|
| `cv` | `cv <value>` | Set CV to 0-255 or `<n>V` (e.g., `cv 2.5V`) |
| `tap` | `tap <target> [duration_ms]` | Press, wait, release (default 200ms) |
| `hold` | `hold <target> <duration_ms>` | Press, wait duration, release |
| `fault` | `fault <subsystem> <mode>` | Inject fault (e.g., `fault adc timeout`) |
| `cv_lfo` | `cv_lfo <freq_hz> <shape>` | Start CV LFO (sine/tri/square) |
| `cv_ramp` | `cv_ramp <from> <to> <duration_ms>` | Ramp CV over time |
| `cv_off` | `cv_off` | Stop LFO/ramp, return to manual |

**Extended Assert Targets:**

| Target | Description |
|--------|-------------|
| `output` | Output pin state (existing) |
| `cv_digital` | Post-hysteresis CV state |
| `mode` | Current mode (gate/trigger/toggle/divide/cycle) |
| `page` | Current menu page |
| `state` | Top-level state (perform/menu) |
| `cv_raw` | Raw ADC value (for fault testing) |

**Example - Improved Script:**
```
# Test CV hysteresis boundaries
0 log Testing hysteresis thresholds

# Set CV just below high threshold (128 = 2.5V)
0 cv 125
100 assert cv_digital low
100 assert output low

# Cross high threshold
200 cv 130
300 assert cv_digital high
300 assert output high

# Drop to hysteresis band (between 77-128) - should stay high
400 cv 100
500 assert cv_digital high

# Drop below low threshold (77 = 1.5V)
600 cv 70
700 assert cv_digital low

# Test with fault injection
800 fault adc timeout
900 assert cv_raw 128

# Test tap gesture
1000 fault adc normal
1000 tap a 200
1300 assert mode trigger  # Mode should have changed
```

### Implementation Details

#### ADC Fault Injection

Modify `sim_adc_read()` in `sim/sim_hal.c`:

```c
static SimAdcMode adc_mode = SIM_ADC_NORMAL;
static uint8_t adc_noise_amplitude = 0;

static uint8_t sim_adc_read(uint8_t channel) {
    if (channel != 3) return 0;

    switch (adc_mode) {
        case SIM_ADC_TIMEOUT:
            return 128;  // Match hardware timeout behavior
        case SIM_ADC_STUCK_LOW:
            return 0;
        case SIM_ADC_STUCK_HIGH:
            return 255;
        case SIM_ADC_NOISY: {
            int16_t noisy = sim_cv_voltage +
                ((rand() % (2 * adc_noise_amplitude + 1)) - adc_noise_amplitude);
            if (noisy < 0) noisy = 0;
            if (noisy > 255) noisy = 255;
            return (uint8_t)noisy;
        }
        case SIM_ADC_NORMAL:
        default:
            return sim_cv_voltage;
    }
}
```

#### Socket Command Extensions

Add fault injection commands to the socket protocol:

```json
{"cmd": "fault_adc", "mode": "timeout"}
{"cmd": "fault_adc", "mode": "noisy", "amplitude": 10}
{"cmd": "fault_adc", "mode": "normal"}
{"cmd": "fault_eeprom", "write_fail": true}
```

#### Integration Test Script Format

Extend the existing script format with fault injection:

```
# cv_adc_timeout_recovery.gks
# Test that system handles ADC timeout gracefully

# Start with normal ADC
0 fault adc normal
0 cv 200
100 assert output high

# Inject ADC timeout (should read as ~2.5V = 128)
200 fault adc timeout
# With timeout, ADC returns 128 which is AT the high threshold
# Hysteresis should keep output HIGH (need to go below 77 to go LOW)
300 assert output high

# Recover from timeout
400 fault adc normal
500 cv 0
600 assert output low
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `sim/sim_hal.c` | Modify | Add fault injection state and API |
| `sim/sim_hal.h` | Modify | Declare fault injection functions |
| `sim/command_handler.c` | Modify | Handle fault injection commands |
| `sim/scripts/cv_gate_chain.gks` | Create | Gate mode integration test |
| `sim/scripts/cv_trigger_chain.gks` | Create | Trigger mode integration test |
| `sim/scripts/cv_menu_passthrough.gks` | Create | Menu mode CV passthrough test |
| `sim/scripts/adc_timeout_recovery.gks` | Create | ADC fault recovery test |
| `sim/scripts/hysteresis_bounds.gks` | Create | Threshold boundary test |
| `docs/SIMULATOR.md` | Create | Simulator documentation |
| `CLAUDE.md` | Modify | Reference new simulator docs |

## Implementation Phases

### Phase 1: ADC Fault Injection (MVP)
- Add `SimAdcMode` enum and state to `sim_hal.c`
- Implement `sim_adc_set_mode()` function
- Add socket command handler for `fault_adc`
- Create `adc_timeout_recovery.gks` test script
- Verify timeout behavior matches hardware (returns 128)

### Phase 2: Integration Test Suite
- Create CV signal chain test scripts
- Create hysteresis boundary tests
- Create menu mode passthrough test
- Add integration tests to CI workflow

### Phase 3: Extended Fault Injection
- Add EEPROM fault injection
- Add timer drift simulation
- Add noise injection mode
- Document all fault injection capabilities

### Phase 4: Documentation
- Create `docs/SIMULATOR.md`
- Document behavioral guarantees
- Add fault injection usage examples
- Update CLAUDE.md with references

## Alternatives Considered

### Hardware-in-the-Loop Testing

Running simulator scripts against real hardware via serial/debug interface.

**Deferred because**: Requires additional infrastructure (debug probe integration, serial protocol). Could be valuable future work but adds complexity. Fault injection in sim is more practical for CI.

### Formal Verification of Signal Paths

Using static analysis or model checking to verify signal flow.

**Rejected because**: Overkill for this project size. Integration tests provide sufficient coverage with much lower effort.

### Separate Test HAL

Creating a third HAL implementation specifically for testing with fault injection.

**Rejected because**: Would duplicate effort. Better to enhance the existing sim_hal which already has the right structure.

## Open Questions

1. **Noise model**: Should `SIM_ADC_NOISY` use uniform random noise or something more realistic (Gaussian, pink noise)? !! realistic

2. **Fault persistence**: Should faults auto-clear after some time, or remain until explicitly cleared? !! remain

3. **Fault logging**: Should fault injection events be logged to the event stream for debugging? !! yes

4. **Script assertions**: Should we extend the script language to assert on intermediate values (e.g., `assert cv_digital true`)? !! let's analyze. I think the script language could use a second look anyway

## References

- [AP-005: Fix Core Signal Processing](../action-plans/AP-005-fix-core-signal-processing.md) - Bug that motivated this analysis
- [ADR-004: Analog CV Input with Software Hysteresis](../decision-records/004-analog-cv-input.md) - CV input design
- [RPT-001: Hardware Testing Findings](../reports/RPT-001-hardware-testing-findings.md) - Hardware bringup issues
- `sim/sim_hal.c` - Current simulator HAL implementation
- `src/hardware/hal.c` - Hardware HAL for behavioral comparison

---

## Addenda
