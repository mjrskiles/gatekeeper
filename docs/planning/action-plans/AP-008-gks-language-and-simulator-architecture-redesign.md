---
type: ap
id: AP-008
status: active
created: 2025-12-23
modified: '2025-12-24'
supersedes: null
superseded_by: null
obsoleted_by: null
related:
- FDP-017
---

# AP-008: GKS Language and Simulator Architecture Redesign

## Status

Active

## Created

2025-12-23

## Goal

Redesign the GKS scripting language and simulator architecture through three sequential FDPs, each building on the previous. The result will be a well-documented language with clean parser architecture and proper module separation.

## Context

The current GKS (Gatekeeper Script) implementation has several issues:

1. **No formal grammar** - Language is defined only in code (`input_source.c`), making it hard to extend or document
2. **Verbose syntax** - Every command requires a timestamp, even for "do this now" operations
3. **No wait command** - Time advancement is implicit, requiring workarounds like `100 log ""`
4. **Monolithic parser** - `input_source.c` mixes keyboard input, script parsing, and script execution
5. **Ad-hoc parsing** - Uses `sscanf` with manual dispatch, no clear lexer/parser separation

This action plan creates three FDPs to address these issues in a logical sequence where each builds on the previous.

## Tasks

### Phase 1: Language Design (FDP-017) ✓

- [x] Create FDP-017: GKS Language Specification v1.5
  - [x] Define formal grammar (EBNF) - `sim/gks.ebnf`
  - [x] Design improved syntax with optional timestamps
  - [x] Add explicit `wait <ms>` command
  - [x] Implicit immediate execution (no timestamp = now)
  - [x] Document all commands, targets, and values
  - [x] Add `def`/`end` for user-defined commands
  - [x] Add `const` with simple arithmetic
  - [x] Add `include` for file composition

### Phase 2: Parser Architecture (FDP-018) ✓

- [x] Create FDP-018: GKS Parser Architecture
  - [x] Design lexer/tokenizer layer (tokens, EOL handling)
  - [x] Design parser layer (recursive descent, AST nodes)
  - [x] Design resolver layer (symbol tables, timeline, includes)
  - [x] Design executor layer (timeline walking, HAL integration)
  - [x] Define clear interfaces between layers
  - [x] Plan error handling with fuzzy suggestions
  - [x] Timer expansion (finite at resolve, infinite at runtime)
  - [x] Flat module system with filename prefixes

### Phase 3: Module Separation (FDP-019)

- [ ] Create FDP-019: Simulator Input Architecture
  - [ ] Separate script engine from input source
  - [ ] Define input source interface (keyboard, script, socket, etc.)
  - [ ] Design event routing between sources and simulator
  - [ ] Plan for multiple concurrent input sources
  - [ ] Consider testability of each component

### Implementation (after FDP approval)

- [ ] Create implementation action plans for each approved FDP
- [ ] Update existing .gks test scripts to new syntax
- [ ] Update SIMULATOR.md documentation

## Notes

**Sequencing rationale**: Language design must come first because parser architecture depends on knowing what to parse. Module separation comes last because it benefits from having a clean parser implementation to work with.

**Scope boundary**: This AP only covers FDP creation. Implementation is separate work tracked by future APs created after FDP approval.

## Completion Criteria

1. FDP-017 created and approved (language design)
2. FDP-018 created and approved (parser architecture)
3. FDP-019 created and approved (module separation)
4. All three FDPs form a coherent design that can be implemented sequentially

## References

- `sim/gks.ebnf` - Formal EBNF grammar (v1.5)
- `sim/input_source.c` - Current parser implementation (to be replaced)
- `docs/SIMULATOR.md` - Simulator documentation
- `sim/scripts/*.gks` - Existing test scripts (10 files)

---

## Addenda

### 2025-12-24: FDP-017 Complete (v1.5 Scope)

Created FDP-017 and formal EBNF grammar with a focused "v1.5" scope:

**Included in v1.5:**
- Optional timestamps (no prefix = immediate)
- `wait` command for explicit time advancement
- `def`/`end` for user-defined command sequences
- `const` with simple arithmetic (+, -, *, /, %)
- `include` for file composition
- HAL-level primitives (`set pin.X`, `set adc.X`)

**Deferred to v2.0:**
- Variables (`let`) and assignment
- Control flow (`if`/`else`, `repeat`)
- Block scoping
- Full expression evaluation in commands

This focused scope provides high utility without the complexity of a full programming language.

### 2025-12-24: FDP-018 Complete

Created FDP-018 defining the parser architecture:

**Four-stage pipeline:**
1. **Lexer** - Tokenize with EOL preserved, case-insensitive keywords
2. **Parser** - Recursive descent producing AST
3. **Resolver** - Symbol tables, const evaluation, timeline building
4. **Executor** - Timeline walking, HAL integration

**Key decisions:**
- Tree-walking interpreter (no bytecode)
- Flat module system (`buttons.press_a` not `buttons.common.helper`)
- Finite timers expand at resolve time, infinite need runtime handling
- Fuzzy matching for "did you mean?" suggestions
- `timer NAME interval count N` syntax for periodic actions
