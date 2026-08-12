# Architecture

This document describes the firmware architecture for Gatekeeper, a
Eurorack utility module built on the ATtiny85 microcontroller.

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Module Descriptions](#module-descriptions)
  - [Coordinator](#coordinator)
  - [FSM Engine](#fsm-engine)
  - [Event Processor](#event-processor)
  - [Mode Handlers](#mode-handlers)
  - [HAL](#hal-hardware-abstraction-layer)
  - [App Initialization](#app-initialization)
  - [CV Input](#cv-input)
  - [Button](#button)
  - [CV Output](#cv-output)
- [Data Flow](#data-flow)
- [Testing](#testing)
- [Build System](#build-system)
- [Memory Constraints](#memory-constraints)
- [Extending the Firmware](#extending-the-firmware)
- [Architecture Decisions](#architecture-decisions)

## Overview

Gatekeeper is a digital gate/trigger processor. It reads button and CV
inputs and produces a 5V digital output. Multiple operating modes transform
the input signal in different ways.

```
                    ┌─────────────────┐
  Button A/B ──────▶│                 │──────▶ CV Output (5V)
                    │   ATtiny85      │
  CV Input ────────▶│   @ 8 MHz       │──────▶ Neopixel LEDs
                    └─────────────────┘
```

## Hardware

**Target**: ATtiny85 @ 8 MHz (internal oscillator, per ADR-001)

See [ADR-001](planning/decision-records/archive/001-rev2-architecture.md) for design rationale.

| Pin | Port | Function        |
|-----|------|-----------------|
| PB0 | 5    | Neopixel data   |
| PB1 | 6    | CV output       |
| PB2 | 7    | Button A (menu/secondary) |
| PB3 | 2    | CV input (analog via ADC) |
| PB4 | 3    | Button B (primary/gate control) |
| PB5 | 1    | RESET           |

Output LED is driven directly from the buffered output circuit, not GPIO.

## Module Descriptions

### Directory Structure

The codebase is split into reusable library code (`lib/`) and application-specific code (`src/`):

```
lib/                    # Reusable library code (headers alongside sources)
├── events/             # Event processor state machine
├── fsm/                # Generic table-driven FSM engine
├── hardware/           # HAL interface (hal_interface.h)
├── input/              # Button debouncing, CV input
├── output/             # CV output, LED animation, Neopixel driver
└── utility/            # Delay, PROGMEM, status utilities

src/                    # Application-specific code (headers alongside sources)
├── config/             # Mode configuration
├── core/               # Coordinator, states
├── hardware/           # HAL implementation (hal.c, hal.h)
├── modes/              # Mode handlers
├── output/             # LED feedback
├── app_init.c/.h       # Startup, settings
└── main.c              # Entry point
```

### Core Modules

| Module | Location | Purpose |
|--------|----------|---------|
| `coordinator` | `src/core/` | Application coordinator - manages FSM hierarchy, routes events |
| `fsm` | `lib/fsm/` | Generic table-driven FSM engine (reusable library) |
| `events` | `lib/events/` | Event processor - button gestures, CV edge detection |
| `mode_handlers` | `src/modes/` | Signal processing modes (Gate, Trigger, Toggle, Divide, Cycle) |
| `cv_input` | `lib/input/` | Analog CV input with software hysteresis |
| `button` | `lib/input/` | Button debouncing and edge detection |
| `hal` | `src/hardware/` | Hardware abstraction layer (ATtiny85 implementation) |
| `hal_interface` | `lib/hardware/` | HAL interface definition |
| `cv_output` | `lib/output/` | CV output behaviors |
| `led_animation` | `lib/output/` | Blink/glow animation engine |
| `neopixel` | `lib/output/` | WS2812B bit-banged driver |
| `led_feedback` | `src/output/` | High-level LED control (mode colors, activity) |
| `app_init` | `src/` | Startup, EEPROM settings, factory reset |

### Coordinator

Location: `src/core/coordinator.c`, `src/core/coordinator.h`

The coordinator is the **main application logic**. It manages a three-level
FSM hierarchy and routes events to the appropriate state machine.

**FSM Hierarchy**:
```
┌─────────────────────────────────────────┐
│              Top FSM                     │
│         PERFORM <──> MENU                │
│                                          │
│  ┌─────────────┐    ┌─────────────┐     │
│  │  Mode FSM   │    │  Menu FSM   │     │
│  │ Gate/Trig/  │    │ Page nav    │     │
│  │ Toggle/Div/ │    │             │     │
│  │ Cycle       │    │             │     │
│  └─────────────┘    └─────────────┘     │
└─────────────────────────────────────────┘
```

**Responsibilities**:
1. Initialize and start all three FSMs
2. Poll inputs via HAL (buttons, CV via ADC)
3. Process inputs through event processor to detect gestures
4. Route events to appropriate FSM based on current state
5. Run mode handlers in PERFORM state
6. Handle menu timeout (60 second auto-exit)

**Update Loop** (`coordinator_update()`):
```c
1. Read CV input via ADC, apply hysteresis
2. Build EventInput struct from button/CV states
3. Process through event processor to get Event
4. Route event to Top FSM (may cascade to Mode/Menu FSM)
5. Check menu timeout
6. In PERFORM state: run mode handler with button B state
```

**Gestures**:
- `EVT_MENU_TOGGLE`: Hold A, then hold B → enter menu
- `EVT_A_HOLD` (in menu): Hold A alone → exit menu immediately
- `EVT_MODE_NEXT`: Hold A alone, then release → cycle to next mode

Note: If B is pressed during an A hold, the solo A gestures (menu exit, mode change)
are cancelled. This prevents accidental triggers when entering menu.

### FSM Engine

Location: `lib/fsm/fsm.c`, `lib/fsm/fsm.h`

Generic table-driven finite state machine engine. Transition tables are
stored in PROGMEM to conserve RAM.

**Key Features**:
- Declarative state/transition tables
- Entry/exit/update actions per state
- Transition actions
- Wildcard state matching (`FSM_ANY_STATE`)
- No-transition actions (action without state change)

**State Definition**:
```c
typedef struct {
    uint8_t state_id;           // State identifier
    FSMAction entry_action;     // Called on state entry
    FSMAction exit_action;      // Called on state exit
    FSMAction update_action;    // Called each tick while in state
} State;
```

**Transition Definition**:
```c
typedef struct {
    uint8_t from_state;         // Current state (or FSM_ANY_STATE)
    uint8_t event;              // Triggering event
    uint8_t to_state;           // Next state (or FSM_NO_TRANSITION)
    FSMAction action;           // Action to execute
} Transition;
```

See [ADR-003](planning/decision-records/archive/003-fsm-state-management.md) for design rationale.

### Event Processor

Location: `lib/events/events.c`, `lib/events/events.h`

Transforms raw button/CV inputs into semantic events with timing for
press vs release, tap vs hold, and compound gestures.

**Event Types**:
| Category | Events |
|----------|--------|
| Performance (immediate) | `EVT_A_PRESS`, `EVT_B_PRESS`, `EVT_CV_RISE`, `EVT_CV_FALL` |
| Configuration (on release) | `EVT_A_TAP`, `EVT_A_RELEASE`, `EVT_B_TAP`, `EVT_B_RELEASE` |
| Hold (solo A only) | `EVT_A_HOLD` (only fires if B not pressed) |
| Hold (B always) | `EVT_B_HOLD` |
| Gestures | `EVT_MENU_TOGGLE` (A+B hold), `EVT_MODE_NEXT` (solo A hold+release) |

**Timing Constants**:
```c
#define EP_HOLD_THRESHOLD_MS    500   // Time to trigger hold event
#define EP_TAP_THRESHOLD_MS     300   // Max duration for tap
```

**State Tracking** (per ADR-002):
Uses status bitmask instead of multiple bools to save RAM:
```c
#define EP_A_PRESSED    (1 << 0)
#define EP_A_LAST       (1 << 1)
#define EP_A_HOLD       (1 << 2)
#define EP_B_PRESSED    (1 << 3)
// ... etc
```

### Mode Handlers

Location: `src/modes/mode_handlers.c`, `src/modes/mode_handlers.h`

Implements the five signal processing modes. Each mode has its own context
struct; they share memory via a union since only one is active at a time.

| Mode    | Behavior |
|---------|----------|
| Gate    | Output follows input directly |
| Trigger | Rising edge triggers fixed-duration pulse (default 10ms) |
| Toggle  | Rising edge flips output state |
| Divide  | Output pulse every N inputs (clock divider, default N=2) |
| Cycle   | Internal clock generator (default 80 BPM) |

**LED Feedback**: Each mode has a distinct color on the mode LED:
- Gate: Green
- Trigger: Cyan
- Toggle: Orange
- Divide: Magenta
- Cycle: Blue

### HAL (Hardware Abstraction Layer)

Location: `src/hardware/hal.c`, `lib/hardware/hal_interface.h`

The HAL provides a swappable interface for hardware access. A global
pointer `p_hal` references the active implementation.

```c
typedef struct {
    // Pin assignments
    uint8_t button_a_pin;
    uint8_t button_b_pin;
    uint8_t sig_out_pin;
    // ... other pins

    // IO functions
    void (*init)(void);
    void (*set_pin)(uint8_t pin);
    void (*clear_pin)(uint8_t pin);
    uint8_t (*read_pin)(uint8_t pin);

    // Timer functions
    uint32_t (*millis)(void);
    void (*delay_ms)(uint32_t ms);

    // ADC functions (per ADR-004)
    uint8_t (*adc_read)(uint8_t channel);

    // EEPROM functions
    uint8_t (*eeprom_read_byte)(uint16_t addr);
    void (*eeprom_write_byte)(uint16_t addr, uint8_t value);
    // ... other functions
} HalInterface;

extern HalInterface *p_hal;
```

**Implementations**:
- Production: `src/hardware/hal.c` (real ATtiny85 hardware)
- Tests: `test/unit/mocks/mock_hal.c` (virtual pins, controllable time)
- Simulator: `sim/sim_hal.c` (native with virtual hardware)

**Timer**: Timer0 runs in CTC mode with prescaler 8, generating a 1ms
interrupt. Uses 16-bit counter in ISR (atomic) with 32-bit extension in
`hal_millis()` for correct overflow handling.

### App Initialization

Location: `src/app_init.c`, `src/app_init.h`

Handles startup tasks before entering the main application loop:

1. **Factory Reset Detection**: If both buttons are held for 3 seconds,
   EEPROM is cleared and defaults are restored
2. **Settings Validation**: Loads and validates settings from EEPROM using
   magic number, schema version, checksum, and range checks
3. **Graceful Degradation**: Falls back to safe defaults on any validation
   failure rather than halting

**EEPROM Layout**:
```
0x00-0x01: Magic number (0x474B = "GK")
0x02:      Schema version
0x03-0x0A: AppSettings struct (8 bytes)
0x10:      XOR checksum
```

See [FDP-001](planning/feature-designs/archive/FDP-001-app-init.md) for detailed design.

### CV Input

Location: `lib/input/cv_input.c`, `lib/input/cv_input.h`

Processes analog CV input with software hysteresis (Schmitt trigger).

**Per ADR-004**:
- Reads 8-bit ADC value (0-255 = 0-5V)
- High threshold: 128 (2.5V) to go HIGH
- Low threshold: 77 (1.5V) to go LOW
- 1V hysteresis band for noise rejection

See [ADR-004](planning/decision-records/004-analog-cv-input.md) for design rationale.

### Button

Location: `lib/input/button.c`, `lib/input/button.h`

Handles debouncing and edge detection for button input.

**State**:
- `pressed`: Debounced button state
- `rising_edge`: True for one cycle after press
- `falling_edge`: True for one cycle after release

**Timing Constants**:
```c
#define EDGE_DEBOUNCE_MS  5
```

### CV Output

Location: `lib/output/cv_output.c`, `lib/output/cv_output.h`

Low-level output pin management. Mode-specific behavior is implemented
in mode_handlers.c; this module provides the output abstraction.

## Data Flow

```
Button A (PB2) ────────────────────────────────────────┐
Button B (PB4) ────────────────────────────────────────┤
CV Input (PB3) ──▶ ADC ──▶ cv_input (hysteresis) ─────┤
                                                       ▼
                                              ┌─────────────────┐
                                              │ Event Processor │
                                              │  (gestures,     │
                                              │   edge detect)  │
                                              └────────┬────────┘
                                                       │ Event
                                                       ▼
                                              ┌─────────────────┐
                                              │   Coordinator   │
                                              │  (FSM routing)  │
                                              └────────┬────────┘
                                                       │
                            ┌──────────────────────────┼──────────────────────────┐
                            │                          │                          │
                            ▼                          ▼                          ▼
                     ┌─────────────┐            ┌─────────────┐            ┌─────────────┐
                     │   Top FSM   │            │  Mode FSM   │            │  Menu FSM   │
                     │ PERFORM/MENU│            │ Gate/Trig/..│            │  Page nav   │
                     └─────────────┘            └──────┬──────┘            └─────────────┘
                                                       │
                                                       ▼
                                              ┌─────────────────┐
                                              │  Mode Handler   │
                                              │  (signal proc)  │
                                              └────────┬────────┘
                                                       │
                            ┌──────────────────────────┴──────────────────────────┐
                            ▼                                                      ▼
                     CV Output (PB1)                                      Neopixel LEDs (PB0)
```

## Testing

Tests run on the host machine using the Unity framework. The mock HAL
provides:

- Virtual pin state array (read/write any pin)
- Virtual millisecond timer (`advance_mock_time()`)
- Non-blocking delay (`mock_delay_ms()` advances time instantly)
- Simulated 512-byte EEPROM (`mock_eeprom_clear()` resets to 0xFF)
- Mock ADC with settable values (`mock_adc_set_value()`)
- Deterministic behavior for timing-dependent tests

**Running tests**:
```sh
cmake --preset tests && cmake --build --preset tests
ctest --preset tests
# Or directly: ./build/tests/test/unit/gatekeeper_unit_tests
```

**Test organization**: Each module has a corresponding test file in
`test/unit/`. Tests cover edge cases, timing boundaries, and integration
between modules.

## Build System

CMake-based build with three configurations using presets:

**Firmware build**:
```sh
cmake --preset firmware && cmake --build --preset firmware
```
Produces `build/firmware/gatekeeper.hex` for flashing.

**Test build**:
```sh
cmake --preset tests && cmake --build --preset tests
```
Compiles with host GCC, defines `TEST_BUILD` macro, links Unity.

**Simulator build**:
```sh
cmake --preset sim && cmake --build --preset sim
```
Compiles native simulator with interactive terminal UI, JSON output, or batch mode.

**Flashing**:
```sh
make flash           # Program hex to device
make fuses           # Set fuse configuration
make read_fuses      # Verify fuses
```

Default programmer: stk500v2 on /dev/ttyACM0.

## Memory Constraints

The ATtiny85 has limited resources:

| Resource | Size   | Notes |
|----------|--------|-------|
| Flash    | 8 KB   | Program code |
| SRAM     | 512 B  | Stack and globals |
| EEPROM   | 512 B  | Persistent storage |

**Strategies**:
- Global HAL pointer avoids passing pointers through call stack
- `-Os` optimization for code size
- `-fshort-enums` for 1-byte enums
- No dynamic allocation
- FSM transition tables in PROGMEM (flash), not RAM
- Status bitmasks instead of multiple bools (per ADR-002)

## Extending the Firmware

### Adding a New Mode

1. Add enum value to `ModeState` in `src/core/states.h`
2. Add context struct to `ModeContext` union in `src/modes/mode_handlers.h`
3. Implement `mode_handler_init()` case in `src/modes/mode_handlers.c`
4. Implement `mode_handler_process()` case
5. Add LED color in `mode_handler_get_feedback()`
6. Add tests in `test/unit/modes/test_mode_handlers.h`

### Adding a New HAL Function

1. Add function pointer to `HalInterface` struct
2. Implement in `src/hardware/hal.c` (production)
3. Implement in `test/unit/mocks/mock_hal.c` (test)
4. Implement in `sim/sim_hal.c` (simulator)
5. Initialize pointer in all HAL instances

### Adding a New Event

1. Add enum value to `Event` in `lib/events/events.h`
2. Add detection logic in `event_processor_update()`
3. Add handling in coordinator or FSM transition tables
4. Add tests in `test/unit/fsm/test_events.h`

## Architecture Decisions

Design decisions are documented in `docs/planning/decision-records/`:

- [ADR-001](planning/decision-records/archive/001-rev2-architecture.md): Rev2 hardware and firmware changes
- [ADR-002](planning/decision-records/archive/002-status-word-consolidation.md): Status bitmasks instead of multiple bools
- [ADR-003](planning/decision-records/archive/003-fsm-state-management.md): Table-driven FSM architecture
- [ADR-004](planning/decision-records/004-analog-cv-input.md): Analog CV input with software hysteresis

## References

- [ATtiny85 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf)
- [AVR Libc Reference](https://www.nongnu.org/avr-libc/user-manual/)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
