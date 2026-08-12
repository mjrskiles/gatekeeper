# Gatekeeper Feature Matrix

> Current capabilities and implementation status

Last updated: 2025-12-21

---

## Platform Support

| Platform | MCU | Status | Notes |
|----------|-----|--------|-------|
| Production | ATtiny85 @ 8MHz | **Supported** | 8KB flash, 512B RAM |
| Unit Tests | Native (host) | **Supported** | Mock HAL, 150 tests |
| Simulator | Native (host) | **Supported** | Interactive + headless, cJSON |

---

## Operating Modes

| Mode | Behavior | Status | LED Color |
|------|----------|--------|-----------|
| Gate | Output follows input | Complete | Green |
| Trigger | Rising edge → fixed pulse | Complete | Cyan |
| Toggle | Rising edge flips output | Complete | Orange |
| Divide | Clock divider (2-24) | Complete | Magenta |
| Cycle | Internal clock generator | Complete | Blue |

### Mode Parameters

| Mode | Parameter | Range | Default | Configurable |
|------|-----------|-------|---------|--------------|
| Trigger | Pulse width | 1-100ms | 10ms | Yes (menu) |
| Trigger | Edge | Rising/Falling | Rising | Yes (menu) |
| Toggle | Edge | Rising/Falling | Rising | Yes (menu) |
| Divide | Divisor | 2-24 | 2 | Yes (menu) |
| Cycle | Tempo | 40-240 BPM | 80 BPM | Yes (menu) |
| Gate | Button A mode | Off/Manual | Off | Yes (menu) |

---

## Input System

### Buttons

| Feature | Status | Notes |
|---------|--------|-------|
| Button A (menu/secondary) | Complete | PB2 |
| Button B (primary/gate) | Complete | PB4 |
| Debouncing (5ms) | Complete | Edge-based |
| Rising edge detection | Complete | Single-cycle pulse |
| Falling edge detection | Complete | Single-cycle pulse |
| Hold detection (500ms) | Complete | For gestures |
| Tap detection (<300ms) | Complete | For menu navigation |

### CV Input

| Feature | Status | Notes |
|---------|--------|-------|
| Analog input (PB3/ADC3) | Complete | 8-bit resolution |
| Software hysteresis | Complete | 1V band (per ADR-004) |
| High threshold | Complete | 2.5V (128/255) |
| Low threshold | Complete | 1.5V (77/255) |
| Digital state output | Complete | For event processor |

### Gestures

| Gesture | Detection | Status |
|---------|-----------|--------|
| Menu toggle | Hold A, then hold B | Complete |
| Mode change | Hold B, then hold A | Complete |
| Page navigation | Tap A (in menu) | Complete |
| Value cycling | Tap B (in menu) | Complete |

---

## Output System

### Gate Output

| Feature | Status | Notes |
|---------|--------|-------|
| 5V gate output (PB1) | Complete | Via buffer circuit |
| Mode-specific behavior | Complete | Per mode handler |

### LED Feedback

| Feature | Status | Notes |
|---------|--------|-------|
| WS2812B Neopixel driver | Complete | Bit-banged, AVR-specific |
| Mode LED (index 0) | Complete | Mode-specific color |
| Activity LED (index 1) | Complete | Output state indicator |
| Blink animation | Complete | Configurable on/off times |
| Glow animation | Complete | Smooth fade up/down |
| Menu page indication | Complete | Activity LED patterns |
| Value indication | Complete | Brightness levels |

---

## Menu System

| Feature | Status | Notes |
|---------|--------|-------|
| Menu entry gesture | Complete | Hold A + Hold B |
| Menu exit gesture | Complete | Same gesture |
| Menu timeout | Complete | 60 seconds |
| Page navigation | Complete | 8 pages |
| Value persistence | Complete | Save on menu exit |

### Menu Pages

| Page | Purpose | Mode-Specific |
|------|---------|---------------|
| PAGE_GATE_CV | Button A mode | Gate |
| PAGE_TRIGGER_BEHAVIOR | Edge selection | Trigger |
| PAGE_TRIGGER_PULSE_LEN | Pulse width | Trigger |
| PAGE_TOGGLE_BEHAVIOR | Edge selection | Toggle |
| PAGE_DIVIDE_DIVISOR | Division ratio | Divide |
| PAGE_CYCLE_PATTERN | Tempo selection | Cycle |
| PAGE_CV_GLOBAL | Global CV settings | All |
| PAGE_MENU_TIMEOUT | Timeout setting | All |

---

## Persistence

### EEPROM Layout

| Address | Content | Status |
|---------|---------|--------|
| 0x00-0x01 | Magic number (0x474B) | Complete |
| 0x02 | Schema version | Complete |
| 0x03-0x0A | AppSettings (8 bytes) | Complete |
| 0x10 | XOR checksum | Complete |

### Settings Validation

| Level | Check | Status |
|-------|-------|--------|
| 1 | Magic number | Complete |
| 2 | Schema version | Complete |
| 3 | Checksum verification | Complete |
| 4 | Range validation | Complete |
| - | Graceful fallback to defaults | Complete |

### Factory Reset

| Feature | Status | Notes |
|---------|--------|-------|
| Button combo detection | Complete | Both buttons, 3 seconds |
| Timer sanity check | Complete | Prevents infinite loop |
| Iteration limit | Complete | 600 max iterations |
| Visual feedback | Complete | LED blink during hold |
| EEPROM clear | Complete | Invalidate magic number |
| Write verification | Complete | Read-back check |

---

## FSM Architecture

| Component | Status | Notes |
|-----------|--------|-------|
| Generic FSM engine | Complete | Table-driven, PROGMEM |
| Top FSM (PERFORM/MENU) | Complete | 2 states |
| Mode FSM | Complete | 5 states |
| Menu FSM | Complete | 8 states |
| Wildcard transitions | Complete | FSM_ANY_STATE |
| Entry/exit actions | Complete | Per state |
| Transition actions | Complete | On state change |

---

## HAL Abstraction

### Interface Functions

| Function | Production | Mock | Simulator |
|----------|------------|------|-----------|
| init() | x | x | x |
| set_pin() | x | x | x |
| clear_pin() | x | x | x |
| toggle_pin() | x | x | x |
| read_pin() | x | x | x |
| millis() | x | x | x |
| delay_ms() | x | x | x |
| advance_time() | - | x | x |
| adc_read() | x | x | x |
| eeprom_read_byte() | x | x | x |
| eeprom_write_byte() | x | x | x |
| eeprom_read_word() | x | x | x |
| eeprom_write_word() | x | x | x |
| wdt_enable() | x | x | x |
| wdt_reset() | x | x | x |
| wdt_disable() | x | x | x |

---

## Simulator Features

### Output Modes

| Mode | Flag | Status | Notes |
|------|------|--------|-------|
| Terminal UI | (default) | Complete | Interactive display |
| JSON | --json | Complete | NDJSON format |
| JSON Stream | --json-stream | Complete | Continuous output |
| Batch | --batch | Complete | Plain text events |

### Input Sources

| Source | Status | Notes |
|--------|--------|-------|
| Keyboard (interactive) | Complete | Raw terminal input |
| Script files (.gks) | Complete | Timed commands |
| Socket commands | Complete | JSON via Unix socket |

### CV Sources (Simulator)

| Source | Status | Notes |
|--------|--------|-------|
| Manual value | Complete | 0-255 direct |
| LFO (sine) | Complete | Configurable freq |
| LFO (triangle) | Complete | |
| LFO (sawtooth) | Complete | |
| LFO (square) | Complete | |
| LFO (random/S&H) | Complete | |
| ADSR envelope | Complete | Gate-triggered |
| Wavetable | Complete | Custom waveforms |

### Socket Server

| Feature | Status | Notes |
|---------|--------|-------|
| Unix domain socket | Complete | /tmp/gatekeeper-sim.sock |
| NDJSON protocol | Complete | One JSON per line |
| State streaming | Complete | 60Hz max |
| Button commands | Complete | Press/release |
| CV source commands | Complete | All source types |
| Reset command | Complete | Time + CV reset |
| Quit command | Complete | Clean shutdown |

---

## Testing

| Category | Tests | Status |
|----------|-------|--------|
| Button debouncing | 12 | Complete |
| Event processor | 24 | Complete |
| FSM engine | 18 | Complete |
| Mode handlers | 35 | Complete |
| Coordinator | 20 | Complete |
| App initialization | 15 | Complete |
| LED feedback | 12 | Complete |
| CV input | 8 | Complete |
| **Total** | **150** | **Complete** |

---

## Resource Usage

| Resource | Used | Available | Percentage |
|----------|------|-----------|------------|
| Flash | ~6.9 KB | 8 KB | ~84% |
| SRAM | ~200 B | 512 B | ~39% |
| EEPROM | 17 B | 512 B | ~3% |

---

## Legend

| Symbol | Meaning |
|--------|---------|
| **Supported** | Production ready |
| Complete | Feature complete |
| Partial | Some functionality |
| Planned | On roadmap |
| Not started | No implementation |
| x | Implemented |
| - | Not applicable |

