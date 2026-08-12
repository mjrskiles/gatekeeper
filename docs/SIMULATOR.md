# Gatekeeper Simulator

The native simulator runs the full Gatekeeper application logic on your host machine, enabling rapid development and testing without hardware.

## Table of Contents

- [Quick Start](#quick-start)
- [Output Modes](#output-modes)
- [Interactive Controls](#interactive-controls)
- [Script Language](#script-language)
- [Fault Injection](#fault-injection)
- [Socket Protocol](#socket-protocol)
- [CI Integration](#ci-integration)
- [Architecture](#architecture)
- [Behavioral Guarantees](#behavioral-guarantees)

## Quick Start

```bash
# Build the simulator
cmake --preset sim && cmake --build --preset sim

# Run interactively (terminal UI)
./build/sim/sim/gatekeeper-sim

# Run a test script
./build/sim/sim/gatekeeper-sim --script sim/scripts/test_gate_mode.gks --batch

# Run all integration tests
./scripts/run_sim_tests.sh
```

## Output Modes

The simulator supports multiple output modes for different use cases:

### Interactive (default)

Terminal UI with real-time state display:

```bash
./build/sim/sim/gatekeeper-sim
```

Shows button states, CV voltage, output state, LED colors, and mode information. Runs in real-time (1ms tick).

### Batch Mode

Plain text output suitable for CI and scripting:

```bash
./build/sim/sim/gatekeeper-sim --batch
./build/sim/sim/gatekeeper-sim --script test.gks --batch
```

Outputs timestamped events and assertion results. Scripts run as fast as possible (no real-time pacing).

### JSON Mode

NDJSON (Newline Delimited JSON) output for parsing and logging:

```bash
./build/sim/sim/gatekeeper-sim --json
```

Each line is a complete JSON object. Schema defined in `sim/schema/sim_state_v1.json`.

Example output:
```json
{"version":1,"timestamp_ms":1234,"state":{"top":"PERFORM","mode":"GATE","page":null},"inputs":{"button_a":false,"button_b":false,"cv_in":false},"outputs":{"signal":true},"leds":[{"index":0,"name":"mode","r":0,"g":255,"b":0}],"events":[]}
```

### Socket Mode

Enable a Unix domain socket for external control:

```bash
./build/sim/sim/gatekeeper-sim --socket
./build/sim/sim/gatekeeper-sim --socket /custom/path.sock
```

Default socket path: `/tmp/gatekeeper-sim.sock`

## Interactive Controls

| Key | Action |
|-----|--------|
| `a` | Tap Button A (press + auto-release after 200ms) |
| `b` | Tap Button B (press + auto-release after 200ms) |
| `A` | Hold Button A (press 'a' again to release) |
| `B` | Hold Button B (press 'b' again to release) |
| `c` / `C` | Toggle CV input (0V ↔ 5V) |
| `+` / `=` | Increase CV voltage (+0.2V) |
| `-` / `_` | Decrease CV voltage (-0.2V) |
| `l` | Cycle LFO preset (off → 1Hz sine → 2Hz tri → 4Hz square → off) |
| `L` | Toggle legend visibility |
| `R` | Reset simulation time |
| `Q` / `ESC` | Quit |

## Script Language

Scripts automate input sequences for testing. Files use the `.gks` extension.

### Syntax

```
# Comment (ignored)
<delay_ms> <action> [target] [value]
@<abs_ms>  <action> [target] [value]   # @ = absolute time
```

Times are in milliseconds. Relative times add to the current time; `@` prefix sets absolute time.

### Actions

| Action | Syntax | Description |
|--------|--------|-------------|
| `press` | `press <target>` | Press button or set CV high |
| `release` | `release <target>` | Release button or set CV low |
| `cv` | `cv <value>` | Set CV to specific ADC value (0-255) |
| `assert` | `assert <target> <high\|low>` | Assert state, fail test if wrong |
| `log` | `log <message>` | Print timestamped message |
| `fault` | `fault <subsystem> <mode>` | Inject hardware fault |
| `quit` | `quit` | End script |

### Targets

| Target | Aliases | Description |
|--------|---------|-------------|
| `a` | `button_a` | Button A |
| `b` | `button_b` | Button B |
| `cv` | `cv_in` | CV input |
| `output` | `out` | Signal output |

### Example Script

```gks
# Test gate mode CV response
1000    log     Starting gate mode test

# Verify initial state
0       cv      0
50      assert  output low

# Apply CV, verify output follows
0       cv      200
50      assert  output high

# Drop CV, verify output follows
0       cv      0
50      assert  output low

100     log     Test passed!
0       quit
```

### CV Values

CV is specified as 0-255 ADC value, mapping to 0-5V:

| ADC Value | Voltage | Notes |
|-----------|---------|-------|
| 0 | 0.0V | Minimum |
| 77 | 1.5V | Low threshold (hysteresis) |
| 128 | 2.5V | High threshold (hysteresis) |
| 255 | 5.0V | Maximum |

Per ADR-004, the CV input uses software hysteresis:
- Goes HIGH when ADC > 128 (above 2.5V)
- Goes LOW when ADC < 77 (below 1.5V)
- Stays in current state when between thresholds

## Fault Injection

The simulator can inject hardware faults for testing error handling and recovery.

### ADC Faults

Simulate ADC hardware failures:

```gks
0       fault   adc timeout      # Returns 128 (mid-scale)
0       fault   adc stuck_low    # Always returns 0
0       fault   adc stuck_high   # Always returns 255
0       fault   adc noisy        # Adds random noise
0       fault   adc normal       # Clear fault
```

| Mode | Behavior |
|------|----------|
| `normal` | Returns actual CV value |
| `timeout` | Returns 128 (matches hardware timeout) |
| `stuck_low` | Always returns 0 |
| `stuck_high` | Always returns 255 |
| `noisy` | Adds ±10 random noise to readings |

### EEPROM Faults

Simulate EEPROM hardware failures:

```gks
0       fault   eeprom write_fail   # Writes silently fail
0       fault   eeprom read_ff      # All reads return 0xFF
0       fault   eeprom corrupt      # Random bit flips on read
0       fault   eeprom normal       # Clear fault
```

| Mode | Behavior |
|------|----------|
| `normal` | Normal operation |
| `write_fail` | Writes silently fail (reads work) |
| `read_ff` | All reads return 0xFF (erased state) |
| `corrupt` | Random bit flip on each read |

### Socket Commands for Faults

```json
{"cmd": "fault_adc", "mode": "timeout"}
{"cmd": "fault_adc", "mode": "noisy", "amplitude": 20}
{"cmd": "fault_adc", "mode": "normal"}

{"cmd": "fault_eeprom", "mode": "write_fail"}
{"cmd": "fault_eeprom", "mode": "normal"}
```

## Socket Protocol

The socket accepts NDJSON commands (one JSON object per line).

### Button Control

```json
{"cmd": "button", "id": "a", "state": true}
{"cmd": "button", "id": "b", "state": false}
```

### CV Control

```json
{"cmd": "cv_manual", "value": 180}
{"cmd": "cv_lfo", "freq_hz": 2.0, "shape": "sine", "min": 0, "max": 255}
{"cmd": "cv_envelope", "attack_ms": 10, "decay_ms": 100, "sustain": 180, "release_ms": 200}
{"cmd": "cv_gate", "state": true}
{"cmd": "cv_trigger"}
```

LFO shapes: `sine`, `triangle` (or `tri`), `sawtooth` (or `saw`), `square`, `random` (or `sh`)

### Fault Injection

```json
{"cmd": "fault_adc", "mode": "timeout"}
{"cmd": "fault_adc", "mode": "noisy", "amplitude": 20}
{"cmd": "fault_eeprom", "mode": "write_fail"}
```

### System Control

```json
{"cmd": "reset"}
{"cmd": "quit"}
```

## CI Integration

### Running All Tests

```bash
./scripts/run_sim_tests.sh
```

This script:
1. Finds all `.gks` files in `sim/scripts/`
2. Runs each in batch mode
3. Reports pass/fail for each
4. Returns non-zero exit code if any test fails

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All assertions passed |
| 1 | One or more assertions failed |
| 2 | Script parse error or other failure |

### Example CI Integration

```yaml
# GitHub Actions example
test-simulator:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Build simulator
      run: cmake --preset sim && cmake --build --preset sim
    - name: Run integration tests
      run: ./scripts/run_sim_tests.sh
```

## Architecture

```
sim/
├── sim_main.c          # Entry point, main loop
├── sim_hal.c/.h        # HAL implementation with fault injection
├── sim_neopixel.c      # Neopixel simulation
├── sim_state.c/.h      # State aggregation for output
├── input_source.c/.h   # Keyboard and script input
├── cv_source.c/.h      # CV generators (manual, LFO, envelope)
├── command_handler.c/.h # Socket command parsing
├── render/             # Output renderers
│   ├── terminal.c      # Interactive terminal UI
│   ├── json.c          # NDJSON output
│   └── batch.c         # Plain text for CI
├── scripts/            # Test scripts (.gks files)
└── schema/             # JSON schema for output
```

### HAL Implementation

The simulator HAL (`sim_hal.c`) provides:
- Virtual pins with read/write
- Simulated millisecond timer
- 512-byte EEPROM simulation
- ADC with configurable CV input
- Watchdog simulation (250ms timeout)
- Fault injection for ADC and EEPROM

### Input Sources

Two input source implementations:
- **Keyboard**: Interactive, real-time (1ms tick with sleep)
- **Script**: Automated, runs as fast as possible

Both implement the `InputSource` interface, allowing the main loop to be input-agnostic.

## Behavioral Guarantees

### Identical to Hardware

- FSM state machine logic
- Event processor gestures and timing
- Mode handler signal processing
- CV input hysteresis thresholds
- EEPROM layout and validation
- Watchdog timeout (250ms)

### Known Differences

| Aspect | Hardware | Simulator |
|--------|----------|-----------|
| Timing precision | ISR-driven 1ms | Host timer (~1ms) |
| Neopixel output | Bit-banged WS2812B | RGB values in memory |
| ADC sampling | Hardware ADC | Direct value injection |
| EEPROM wear | Limited cycles | Unlimited |
| Real-time | Hard real-time | Best-effort |

### When to Use Each

**Use Simulator for:**
- Rapid iteration during development
- Testing FSM logic and mode handlers
- Verifying gesture recognition
- Testing fault recovery
- CI/automated testing
- Exploring behavior without hardware

**Use Hardware for:**
- Final validation
- Timing-critical behavior
- Analog signal quality
- Power consumption
- Production verification

## References

- [ADR-004: Analog CV Input](planning/decision-records/004-analog-cv-input.md) - CV thresholds
- [FDP-016: Simulator Fidelity](planning/feature-designs/FDP-016-simulator-fidelity-and-fault-injection.md) - Fault injection design
- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall firmware architecture
