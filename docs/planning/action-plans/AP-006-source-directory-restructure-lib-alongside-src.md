---
type: ap
id: AP-006
status: completed
created: 2025-12-21
modified: '2025-12-21'
supersedes: null
superseded_by: null
obsoleted_by: null
related: []
---

# AP-006: Source Directory Restructure - lib/ alongside src/

## Status

Completed

## Created

2025-12-21

## Goal

Separate reusable library code from application-specific code by creating a `lib/` directory alongside `src/`. This follows embedded C idioms (PlatformIO, Mutable Instruments) and prepares reusable components for eventual extraction to Sound Byte Libs.

## Context

The current `src/` directory mixes reusable library code (FSM engine, button debouncing, LED animation) with application-specific code (mode handlers, coordinator). Dependency analysis shows ~80% of the codebase is genuinely reusable.

**Modules moving to lib/ (no refactoring needed):**
- `fsm/` - Generic table-driven FSM engine
- `utility/` - progmem.h, status.h, delay.c/h
- `input/button` - Debouncing with edge detection
- `input/cv_input` - Schmitt trigger / hysteresis algorithm
- `events/` - Event processor state machine
- `output/cv_output` - Gate/pulse/toggle output modes
- `output/led_animation` - Blink/glow animation engine
- `output/neopixel` - WS2812B driver (platform-specific)

**Modules staying in src/ (app-specific):**
- `core/` - Coordinator, states.h
- `modes/` - Gate/Trigger/Toggle/Divide/Cycle handlers
- `output/led_feedback` - Coupled to app state enums
- `hardware/` - HAL implementation
- `app_init`, `config/`, `main.c`

**Coupling analysis:**
- No circular dependencies blocking the move
- `led_feedback` depends on `core/states.h` (keep in src/)
- HAL interface could live in lib/, implementation in src/

## Tasks

### Phase 1: Directory Structure

- [ ] **1.1** Create lib/ directory structure:
  ```
  lib/
  ├── fsm/
  ├── utility/
  ├── input/
  ├── events/
  └── output/
  ```

- [ ] **1.2** Create lib/include/ for library headers (mirrors lib/ structure)

### Phase 2: Move Library Code

- [ ] **2.1** Move FSM engine:
  - `src/fsm/fsm.c` → `lib/fsm/fsm.c`
  - `include/fsm/fsm.h` → `lib/include/fsm/fsm.h`

- [ ] **2.2** Move utility modules:
  - `include/utility/progmem.h` → `lib/include/utility/progmem.h`
  - `include/utility/status.h` → `lib/include/utility/status.h`
  - `src/utility/delay.c` → `lib/utility/delay.c`
  - `include/utility/delay.h` → `lib/include/utility/delay.h`

- [ ] **2.3** Move input modules:
  - `src/input/button.c` → `lib/input/button.c`
  - `include/input/button.h` → `lib/include/input/button.h`
  - `src/input/cv_input.c` → `lib/input/cv_input.c`
  - `include/input/cv_input.h` → `lib/include/input/cv_input.h`

- [ ] **2.4** Move events module:
  - `src/events/events.c` → `lib/events/events.c`
  - `include/events/events.h` → `lib/include/events/events.h`

- [ ] **2.5** Move output modules (except led_feedback):
  - `src/output/cv_output.c` → `lib/output/cv_output.c`
  - `include/output/cv_output.h` → `lib/include/output/cv_output.h`
  - `src/output/led_animation.c` → `lib/output/led_animation.c`
  - `include/output/led_animation.h` → `lib/include/output/led_animation.h`
  - `src/output/neopixel.c` → `lib/output/neopixel.c`
  - `include/output/neopixel.h` → `lib/include/output/neopixel.h`

### Phase 3: HAL Interface Decision

- [ ] **3.1** Decide HAL interface location:
  - **Option A**: Keep `include/hardware/hal_interface.h` in src/ (simpler)
  - **Option B**: Move interface to `lib/include/hardware/hal_interface.h`, keep impl in src/
  - Recommendation: Option A for now (less churn)

### Phase 4: Update Build System

- [ ] **4.1** Update CMakeLists.txt for firmware build:
  - Add `lib/` to source file globs
  - Add `lib/include/` to include paths

- [ ] **4.2** Update CMakeLists.txt for test build:
  - Add `lib/` sources
  - Add `lib/include/` to include paths

- [ ] **4.3** Update CMakeLists.txt for simulator build:
  - Add `lib/` sources
  - Add `lib/include/` to include paths

### Phase 5: Update Include Paths

- [ ] **5.1** Update all `#include` statements in src/ files to find headers in new locations
  - May need relative paths or updated include directories
  - Verify no broken includes

- [ ] **5.2** Update test files to find lib/ headers

- [ ] **5.3** Update simulator files to find lib/ headers

### Phase 6: Verification

- [ ] **6.1** Build firmware: `cmake --preset firmware && cmake --build --preset firmware`
- [ ] **6.2** Run tests: `cmake --preset tests && cmake --build --preset tests && ctest --preset tests`
- [ ] **6.3** Build simulator: `cmake --preset sim && cmake --build --preset sim`
- [ ] **6.4** Flash and verify on hardware

### Phase 7: Documentation

- [ ] **7.1** Update ARCHITECTURE.md with new directory structure
- [ ] **7.2** Update CLAUDE.md with new module locations
- [ ] **7.3** Update README.md if needed

## Notes

**Include path strategy:**
- Keep includes as `#include "fsm/fsm.h"` (no `lib/` prefix)
- CMake adds both `lib/include/` and `include/` to include paths
- This allows future extraction without changing any includes

**Neopixel platform specificity:**
- Currently has `#if defined(__AVR__)` guards
- Mock exists in test/mocks and sim/
- No refactoring needed, just move as-is

**No code changes required:**
- All moves are pure file relocations
- Include paths handled by CMake
- No API changes, no refactoring

## Completion Criteria

- [ ] All library code in `lib/`, app code in `src/`
- [ ] All three builds pass (firmware, tests, simulator)
- [ ] All 152 tests pass
- [ ] Firmware runs correctly on hardware
- [ ] Documentation updated

## References

- Mutable Instruments stmlib pattern
- PlatformIO lib/ convention
- Dependency analysis from Claude exploration (2025-12-21)

---

## Addenda

### 2025-12-22: Final Implementation

The restructure was completed with a simpler approach than originally planned:

**Headers alongside sources (no separate include directories)**

Instead of creating `lib/include/` and moving `include/` to `src/include/`, we placed headers directly alongside their source files in both `lib/` and `src/`. This provides:

- One consistent pattern throughout the codebase
- Simpler navigation (header is always next to implementation)
- Better for a learning-focused reference project

**Final structure:**
```
lib/                         src/
├── events/events.c/.h       ├── config/mode_config.h
├── fsm/fsm.c/.h             ├── core/coordinator.c/.h, states.h
├── hardware/hal_interface.h ├── hardware/hal.c/.h
├── input/button.c/.h        ├── modes/mode_handlers.c/.h
│       cv_input.c/.h        ├── output/led_feedback.c/.h
├── output/cv_output.c/.h    ├── app_init.c/.h
│        led_animation.c/.h  └── main.c
│        neopixel.c/.h
└── utility/delay.c/.h
         progmem.h, status.h
```

CMake include directories: `src` and `lib` (no `/include` subdirs).
