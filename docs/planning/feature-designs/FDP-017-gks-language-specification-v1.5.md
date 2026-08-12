---
type: fdp
id: FDP-017
status: proposed
created: 2025-12-23
modified: 2025-12-24
supersedes: null
superseded_by: null
obsoleted_by: null
related: [AP-008]
---

# FDP-017: GKS Language Specification v1.5

## Status

Proposed

## Summary

Redesign the GKS (Gatekeeper Script) language with a formal EBNF grammar, low-level hardware primitives, user-defined commands, and file composition. This is a focused "v1.5" design that addresses the highest-value improvements without adding full programming language features.

## Motivation

The current GKS implementation has several issues:

1. **No formal grammar** - Syntax defined only in C code (`input_source.c`), making it hard to document, extend, or build alternative implementations
2. **Verbose timestamps** - Every command requires a time prefix, even for immediate operations
3. **Missing wait command** - Time advancement is implicit, requiring workarounds
4. **High-level primitives** - Commands like `press` hide hardware details (active-low logic, pin mapping)
5. **No composition** - Can't build reusable command libraries
6. **No user-defined commands** - Must modify C code to add new operations

### Design Goals

- **Formal specification** - EBNF grammar as source of truth
- **HAL-level primitives** - Direct mapping to hardware operations
- **User extensibility** - `def`/`end` for building abstractions
- **Composition** - `include` for reusable libraries
- **Constants** - `const` for named values with simple arithmetic
- **Minimal core** - No built-in sugar; users build what they need
- **C++ implementation** - Better suited for parsing than C

### Explicitly Deferred (v2.0)

These features are intentionally deferred to keep v1.5 simple:
- Variables (`let`) and assignment
- Control flow (`if`/`else`/`end`, `repeat`/`end`)
- Block scoping
- Full expression evaluation in commands

## Detailed Design

### Grammar

The formal grammar is defined in `sim/gks.ebnf`. Key elements:

#### Time Model

```gks
# No prefix = execute now
set pin.a low

# wait = advance time before next command
wait 100
set pin.a high

# @N = execute at absolute time N ms (since sim start)
@0    set pin.a high
@500  set pin.a low
@1000 assert pin.output high

# +N or bare N = relative delay
+50 set pin.b low
50 set pin.b high    # same as +50
```

#### Hardware Primitives

```gks
# SET - inject hardware state
set pin.a low           # digital pin (active-low: low = pressed)
set pin.b high          # digital pin (high = released)
set pin.output high     # output pin
set adc.cv 128          # ADC value (0-255)
set adc.cv 2.5v         # voltage syntax (converted to ADC)

# ASSERT - verify state
assert pin.output high
assert adc.cv 128

# TRIGGER - inject hardware events
trigger wdt             # watchdog timeout
trigger reset           # MCU reset
trigger int.timer0      # timer interrupt
```

#### User-Defined Commands

```gks
# Define reusable command sequences
def press_a
  set pin.a low         # active-low
end

def release_a
  set pin.a high
end

def tap_a
  press_a
  wait 50
  release_a
end

# Use defined commands
tap_a
wait 100
assert pin.output low
```

#### Composition

```gks
# Include library files
include "stdlib.gks"
include "../common/buttons.gks"

# Use definitions from included files
tap_a
hold_b 500
```

#### Fault Injection

```gks
fault adc timeout       # ADC returns mid-scale
fault eeprom write_fail # writes silently fail
fault adc normal        # clear fault
```

#### Inspection

```gks
inspect state           # dump full sim state
inspect memory 0x60     # read memory location
inspect register SREG   # read CPU register
inspect pin.output      # read pin state
```

#### Constants

```gks
# Constants - file-scoped, immutable
const THRESHOLD = 128
const DEBOUNCE_MS = 50
const CV_MID = 255 / 2

# Simple arithmetic in const expressions
const HYSTERESIS = THRESHOLD - 51
```

#### Script Control

```gks
quit                    # success exit (code 0)
fail "Unexpected state" # failure exit (code 1)
break                   # exit def early
```

### Semantic Rules

1. **Time is monotonic** - `@N` must be >= current time
2. **Two-pass parsing** - Definitions collected first, forward references allowed
3. **No recursion** - Definitions cannot call themselves
4. **No redefinition** - Same name cannot be defined twice
5. **Case insensitive keywords** - `SET`, `Set`, `set` all valid
6. **Case sensitive identifiers** - User-defined names are case-sensitive
7. **Unityped** - All values are integers (high=1, low=0)

### Scoping Rules

| Binding | Scope | Mutability |
|---------|-------|------------|
| `const` | File (visible after declaration) | Immutable |
| `def` | File (forward refs OK via two-pass) | N/A |

Note: v1.5 has no block scoping. All bindings are file-scoped.

### Voltage Conversion

| Voltage | ADC Value | Notes |
|---------|-----------|-------|
| 0.0V | 0 | Minimum |
| 1.5V | 77 | Low hysteresis threshold |
| 2.5V | 128 | High hysteresis threshold |
| 5.0V | 255 | Maximum |

Formula: `adc = (voltage / 5.0) * 255`

### Pin Mapping

| Name | AVR Pin | Function | Active |
|------|---------|----------|--------|
| `pin.a` | PB2 | Button A | Low |
| `pin.b` | PB4 | Button B | Low |
| `pin.output` | PB1 | Signal out | High |

### Standard Library (Future)

A `stdlib.gks` could provide common abstractions:

```gks
# stdlib.gks - common button operations

def press_a
  set pin.a low
end

def release_a
  set pin.a high
end

def press_b
  set pin.b low
end

def release_b
  set pin.b high
end

def tap_a
  press_a
  wait 50
  release_a
end

def tap_b
  press_b
  wait 50
  release_b
end

def cv_high
  set adc.cv 200
end

def cv_low
  set adc.cv 50
end

def cv_mid
  set adc.cv 128
end
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `sim/gks.ebnf` | Create | Formal EBNF grammar specification |
| `sim/gks/` | Create | New directory for GKS parser (C++) |
| `sim/gks/lexer.hpp` | Create | Tokenizer |
| `sim/gks/parser.hpp` | Create | Recursive descent parser |
| `sim/gks/ast.hpp` | Create | AST node types |
| `sim/gks/executor.hpp` | Create | Script executor |
| `sim/gks/error.hpp` | Create | Error types and formatting |
| `sim/stdlib.gks` | Create | Standard library definitions |
| `sim/input_source.c` | Delete | Replaced by C++ implementation |
| `sim/scripts/*.gks` | Modify | Update to new syntax |

## Implementation Phases

### Phase 1: Lexer and Core Parser
- C++ lexer (tokenizer) with whitespace/comment handling
- Literals: integers (decimal, hex, binary), voltage, high/low
- Error reporting with line numbers and context

### Phase 2: Constants and Expressions
- `const` declarations
- Simple arithmetic expressions (+, -, *, /, %) for const only
- Constant reference resolution

### Phase 3: Commands
- Hardware commands: `set`, `assert`, `wait`, `log`, `quit`, `fail`
- Time prefixes: `@N`, `+N`, bare `N`, none
- `trigger` for interrupts/resets
- `inspect` for state queries
- `fault` for fault injection

### Phase 4: Definitions and Composition
- `def`/`end` blocks with two-pass parsing
- `include` command with circular detection
- Forward reference resolution
- Standard library (`stdlib.gks`)

### Phase 5: Integration
- Update `.gks` test scripts to new syntax
- Update `SIMULATOR.md` documentation
- Delete old C parser code

## Alternatives Considered

### 1. Parser Generator (flex/bison, ANTLR)

**Rejected**: Adds build dependency, grammar is simple enough for hand-written recursive descent. Parser generators also make error messages harder to customize.

### 2. Forth-style Stack Language

**Considered but deferred**: Interesting for extensibility, but postfix notation is less readable for test scripts. Could revisit if we need more dynamic behavior.

### 3. Keep C Implementation

**Rejected**: C string handling is error-prone, and we want better abstractions (std::string, std::vector, std::variant) for the AST and error handling.

### 4. Python/Lua Embedded Interpreter

**Rejected**: Adds significant runtime dependency. GKS is intentionally simple; full scripting language is overkill.

## Open Questions

1. **Parameters for definitions?** - Should `def tap(target)` be supported? Deferred to v2.0.

2. **Include search paths?** - How do we locate standard library files? Options:
   - Relative to current script only (simplest)
   - Environment variable (`GKS_PATH`)
   - Compiled-in default paths

3. **Integer size?** - 32-bit is sufficient for our use case.

## References

- `sim/gks.ebnf` - Formal grammar specification (v1.5)
- `sim/input_source.c` - Current parser implementation (to be replaced)
- `docs/SIMULATOR.md` - Simulator documentation
- AP-008 - Parent action plan for simulator redesign

---

## Addenda
