# Gatekeeper

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A digital gate/trigger processor for Eurorack modular synthesizers. Gatekeeper has a two-state (0 or 5V) output driven by push button and CV input. The input response is configurable via menu settings. 

This project is designed as a reference for hackers of all types. We assume hardware and coding knowledge, but those with a strong desire to learn and build will find this an approachable beginner hardware project.

## Features

- **Five Operating Modes:**
  - **Gate:** Output stays high while input is held
  - **Trigger:** Rising edge triggers a fixed-duration pulse (1-50ms)
  - **Toggle:** Each press flips the output state
  - **Divide:** Clock divider - outputs every N rising edges (2-24)
  - **Cycle:** Internal clock generator

- **Persistent Settings:**
  Mode selection is saved to EEPROM and restored on power-up. Factory reset is available by holding both buttons for 3 seconds during startup.

- **Debounced Input:**
  Robust button state detection with rising and falling edge detection.

- **Configuration Gestures:**
  - **Menu Enter:** Hold Button A, then hold Button B (500ms each) to enter menu
  - **Menu Exit:** Hold Button A (500ms) to exit menu immediately
  - **Mode Change:** Hold Button A (500ms), then release to cycle modes
  - **Menu Navigation:** Tap Button A to change page, tap Button B to cycle values

- **Hardware Abstraction Layer (HAL):**
  Clean interface for hardware access, enabling comprehensive unit testing on the host machine.

- **Unit Testing:**
  152 tests using the Unity framework verify functionality without hardware.

- **Native Simulator:**
  Interactive terminal UI or headless JSON output for development without hardware.

## Hardware

**Target:** ATtiny85 @ 8 MHz (internal RC oscillator)

| Resource | Size | Usage |
|----------|------|-------|
| Flash | 8 KB | ~87% used |
| SRAM | 512 B | ~38% used |
| EEPROM | 512 B | 17 bytes used (~3%) |

**Pin Assignment:**

| Pin | Function |
|-----|----------|
| PB0 | Neopixel data |
| PB1 | CV output |
| PB2 | Button A (menu/secondary) |
| PB3 | CV input (analog via ADC) |
| PB4 | Button B (primary/gate control) |
| PB5 | RESET |

LED X and Y are Neopixels connected to PB0.

## Code Architecture

The firmware is organized into reusable library code (`lib/`) and application-specific code (`src/`), with headers alongside sources:

```
lib/                    src/
├── events/             ├── config/
├── fsm/                ├── core/
├── hardware/           ├── hardware/
├── input/              ├── modes/
├── output/             ├── output/
└── utility/            ├── app_init.c/.h
                        └── main.c
```

Key modules:

| Module | Purpose |
|--------|---------|
| `core/` | Application coordinator - manages FSM hierarchy, routes events |
| `fsm/` | Generic table-driven FSM engine (reusable library) |
| `events/` | Event processor - button gestures, CV edge detection |
| `modes/` | Signal processing modes (Gate, Trigger, Toggle, Divide, Cycle) |
| `hardware/` | HAL implementation (pin control, timers, EEPROM) |
| `input/` | Button debouncing and edge detection |
| `output/` | CV output behaviors |
| `app_init.c` | Startup sequence, settings validation, factory reset |

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed documentation including data flow diagrams, EEPROM layout, and extension guides.

## Build Instructions

### Prerequisites

**Firmware build:**
- AVR-GCC toolchain (`avr-gcc`, `avr-objcopy`, `avr-size`, `avr-strip`)
- CMake 3.21+
- Optional: `avrdude` (flashing), `bc` + `avr-nm` (size analysis)

**Test/Simulator build** (no AVR toolchain needed):
- GCC (host compiler)
- CMake 3.21+

Install on Debian/Ubuntu:
```bash
# Tests only
sudo apt install build-essential cmake

# Firmware (minimal)
sudo apt install gcc-avr avr-libc cmake

# Firmware (full: build + flash + analysis)
sudo apt install gcc-avr avr-libc binutils-avr avrdude cmake bc
```

### Quick Start

```bash
# Build firmware
cmake --preset firmware && cmake --build --preset firmware

# Build and run tests
cmake --preset tests && cmake --build --preset tests
ctest --preset tests

# Build and run simulator
cmake --preset sim && cmake --build --preset sim
./build/sim/sim/gatekeeper-sim
```

### Native Simulator

The simulator is under construction.

### Flashing

```bash
make flash        # Program firmware
make fuses        # Set fuse configuration
make read_fuses   # Verify fuses
```

Default programmer: stk500v2 on `/dev/ttyACM0`. Override with:

```bash
cmake -DPROGRAMMER=usbasp -DPROGRAMMER_PORT=/dev/ttyUSB0 ..
```

## License

MIT
