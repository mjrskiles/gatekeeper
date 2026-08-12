# FDP-015: cJSON Integration for Simulator

## Status

Implemented (2025-12-21)

## Summary

Replace the hand-rolled JSON parser and serializer in the simulator with cJSON, a lightweight MIT-licensed JSON library. This improves robustness, reduces maintenance burden, and provides a familiar API for future development.

## Motivation

The simulator currently uses ~350 lines of hand-written JSON code:

| File | Lines | Purpose |
|------|-------|---------|
| `command_handler.c` | ~170 | Parse incoming JSON commands |
| `render_json.c` | ~60 | Serialize state to JSON output |

**Problems with current approach:**

1. **Edge cases:** The hand-rolled parser doesn't handle all JSON features (Unicode escapes `\uXXXX`, null values, scientific notation)
2. **Maintenance:** Adding new command fields requires parser updates
3. **Error messages:** Limited diagnostic information on parse failures
4. **Testing burden:** Parser bugs are our responsibility to find and fix

**Why cJSON:**

- MIT licensed (permissive, compatible with project)
- Single header + source file (~1600 lines total)
- Well-tested, widely used (17k+ GitHub stars)
- Familiar API that Michael already knows
- Supports both parsing and serialization
- No external dependencies

**Why now:**

The simulator is a development tool, not production firmware. The zero-dependency constraint that applies to ATtiny85 code doesn't apply here. Using a proven library is the pragmatic choice.

## Detailed Design

### Overview

1. Add cJSON as a git submodule in `external/cJSON/`
2. Replace `command_handler.c` parser with cJSON parsing
3. Replace `render_json.c` printf serialization with cJSON building
4. Remove hand-rolled parser code

### Integration Method

**Git Submodule (Implemented)**

```bash
git submodule add https://github.com/DaveGamble/cJSON.git external/cJSON
```

Pros: Version-controlled, easy updates, clear provenance
Cons: Requires `git submodule update --init` after clone

The simulator build will fail with a helpful error message if the submodule is not initialized.

### CMake Integration

```cmake
# sim/CMakeLists.txt additions

# Add cJSON library
add_library(cjson STATIC
    ${CMAKE_SOURCE_DIR}/external/cJSON/cJSON.c
)
target_include_directories(cjson PUBLIC
    ${CMAKE_SOURCE_DIR}/external/cJSON
)

# Link to simulator
target_link_libraries(gatekeeper-sim PRIVATE cjson)
```

### Command Handler Changes

**Before (hand-rolled):**

```c
// Parse command type
char cmd[32];
if (!get_string(json, "cmd", cmd, sizeof(cmd))) {
    snprintf(result.error, sizeof(result.error), "missing 'cmd' field");
    return result;
}

// Parse button command
char id[16];
bool state;
if (!get_string(json, "id", id, sizeof(id))) { ... }
if (!get_bool(json, "state", &state)) { ... }
```

**After (cJSON):**

```c
#include "cJSON.h"

CommandResult command_handler_execute(const char *json_str, CVSource *cv_source) {
    CommandResult result = { CMD_UNKNOWN, false, false, "" };

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        snprintf(result.error, sizeof(result.error),
                 "JSON parse error near: %.20s", error_ptr ? error_ptr : "unknown");
        return result;
    }

    // Get command type
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(json, "cmd");
    if (!cJSON_IsString(cmd) || !cmd->valuestring) {
        snprintf(result.error, sizeof(result.error), "missing 'cmd' field");
        cJSON_Delete(json);
        return result;
    }

    // Dispatch based on command
    if (strcmp(cmd->valuestring, "button") == 0) {
        result = handle_button(json);
    } else if (strcmp(cmd->valuestring, "cv_manual") == 0) {
        result = handle_cv_manual(json, cv_source);
    }
    // ... other commands

    cJSON_Delete(json);
    return result;
}

static CommandResult handle_button(cJSON *json) {
    CommandResult result = { CMD_BUTTON, false, false, "" };

    cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");

    if (!cJSON_IsString(id)) {
        snprintf(result.error, sizeof(result.error), "missing 'id' field");
        return result;
    }
    if (!cJSON_IsBool(state)) {
        snprintf(result.error, sizeof(result.error), "missing 'state' field");
        return result;
    }

    if (strcmp(id->valuestring, "a") == 0) {
        sim_set_button_a(cJSON_IsTrue(state));
        result.success = true;
    } else if (strcmp(id->valuestring, "b") == 0) {
        sim_set_button_b(cJSON_IsTrue(state));
        result.success = true;
    } else {
        snprintf(result.error, sizeof(result.error), "invalid button id: %s", id->valuestring);
    }

    return result;
}
```

### JSON Renderer Changes

**Before (printf):**

```c
static void json_render(Renderer *self, const SimState *state) {
    printf("{");
    printf("\"version\":%d,", state->version);
    printf("\"timestamp_ms\":%lu,", (unsigned long)state->timestamp_ms);
    printf("\"state\":{");
    printf("\"top\":\"%s\",", sim_top_state_str(state->top_state));
    // ... many more printf calls
    printf("}\n");
    fflush(stdout);
}
```

**After (cJSON):**

```c
#include "cJSON.h"

static void json_render(Renderer *self, const SimState *state) {
    cJSON *root = cJSON_CreateObject();

    // Version and timestamp
    cJSON_AddNumberToObject(root, "version", state->version);
    cJSON_AddNumberToObject(root, "timestamp_ms", state->timestamp_ms);

    // State object
    cJSON *state_obj = cJSON_AddObjectToObject(root, "state");
    cJSON_AddStringToObject(state_obj, "top", sim_top_state_str(state->top_state));
    cJSON_AddStringToObject(state_obj, "mode", sim_mode_str(state->mode));
    if (state->in_menu) {
        cJSON_AddStringToObject(state_obj, "page", sim_page_str(state->page));
    } else {
        cJSON_AddNullToObject(state_obj, "page");
    }

    // Inputs object
    cJSON *inputs = cJSON_AddObjectToObject(root, "inputs");
    cJSON_AddBoolToObject(inputs, "button_a", state->button_a);
    cJSON_AddBoolToObject(inputs, "button_b", state->button_b);
    cJSON_AddBoolToObject(inputs, "cv_in", state->cv_in);

    // Outputs object
    cJSON *outputs = cJSON_AddObjectToObject(root, "outputs");
    cJSON_AddBoolToObject(outputs, "signal", state->signal_out);

    // LEDs array
    cJSON *leds = cJSON_AddArrayToObject(root, "leds");
    const char* led_names[] = {"mode", "activity"};
    for (int i = 0; i < SIM_NUM_LEDS; i++) {
        cJSON *led = cJSON_CreateObject();
        cJSON_AddNumberToObject(led, "index", i);
        cJSON_AddStringToObject(led, "name", led_names[i]);
        cJSON_AddNumberToObject(led, "r", state->leds[i].r);
        cJSON_AddNumberToObject(led, "g", state->leds[i].g);
        cJSON_AddNumberToObject(led, "b", state->leds[i].b);
        cJSON_AddItemToArray(leds, led);
    }

    // Events array
    cJSON *events = cJSON_AddArrayToObject(root, "events");
    // ... add events similar to current implementation

    // Print and cleanup
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);
    fflush(stdout);

    free(json_str);
    cJSON_Delete(root);
}
```

### Memory Considerations

cJSON allocates memory for parsed structures and built objects. This is acceptable for the simulator:

- Simulator runs on host machine with ample memory
- Objects are short-lived (parse → use → delete)
- No memory constraints like ATtiny85

### Error Handling

cJSON provides better error diagnostics:

```c
cJSON *json = cJSON_Parse(input);
if (!json) {
    const char *error_ptr = cJSON_GetErrorPtr();
    // error_ptr points to location in input where parsing failed
}
```

This replaces the current generic "parse error" messages with specific location information.

### Testing Strategy

1. **Existing tests:** All simulator tests should continue to pass
2. **JSON edge cases:** Add tests for previously-unsupported JSON features:
   - Unicode escapes: `{"cmd": "button", "id": "\u0061"}`
   - Scientific notation: `{"value": 1.5e2}`
   - Null values: `{"optional": null}`
3. **Error handling:** Test malformed JSON produces meaningful errors
4. **Memory:** Run with valgrind to verify no leaks

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `external/cJSON/` | Create | Git submodule pointing to cJSON repo |
| `.gitmodules` | Create | Git submodule configuration |
| `sim/CMakeLists.txt` | Modify | Add cJSON library, link to simulator, update comments |
| `sim/command_handler.c` | Rewrite | Replace hand-rolled parser with cJSON |
| `sim/render/render_json.c` | Rewrite | Replace printf serialization with cJSON |
| `sim/schema/sim_state_v1.json` | Modify | Update description (x86 → native) |

### Code Removed

The following hand-rolled parser code was deleted from `command_handler.c`:

- `skip_ws()` - whitespace skipping
- `parse_string()` - string parsing
- `parse_number()` - number parsing
- `parse_bool()` - boolean parsing
- `find_key()` - object key lookup
- `get_string()` - string extraction helper
- `get_number()` - number extraction helper
- `get_bool()` - boolean extraction helper

Similarly, `json_escape_string()` was removed from `render_json.c`.

## Dependencies

- **cJSON** (MIT License): https://github.com/DaveGamble/cJSON
  - Version: Latest stable (currently 1.7.17)
  - Files needed: `cJSON.c`, `cJSON.h`, `LICENSE`

## Alternatives Considered

### 1. Keep Hand-Rolled Parser

Continue maintaining custom JSON code.

**Rejected:** Maintenance burden, edge case bugs, no benefit over proven library.

### 2. jsmn (Minimalist Tokenizer)

Zero-allocation JSON tokenizer, ~400 lines.

**Rejected:** Lower-level API requires more glue code. cJSON's object-oriented API is more ergonomic for our use case.

### 3. yyjson (High Performance)

Very fast JSON library, MIT licensed.

**Rejected:** Overkill for simulator. Performance is not a bottleneck. cJSON is simpler and sufficient.

### 4. parson

Simple JSON library, MIT licensed.

**Considered:** Good alternative. Chose cJSON due to wider adoption and Michael's familiarity.

### 5. nlohmann/json (C++)

Popular C++ JSON library.

**Rejected:** Would require changing simulator to C++. Not worth the migration cost.

## Open Questions

1. **Submodule vs vendored:** Start with vendored copy. Migrate to submodule if we want automatic updates.

2. **cJSON configuration:** cJSON has optional features (comments, etc). Use defaults unless specific needs arise.

3. **Print formatting:** Use `cJSON_PrintUnformatted()` for compact output (current behavior) or `cJSON_Print()` for pretty-printed (useful for debugging)?

## Implementation Checklist

- [x] Add cJSON as git submodule in `external/cJSON/`
- [x] Update `sim/CMakeLists.txt` to build and link cJSON
- [x] Refactor `command_handler.c` to use cJSON for parsing
- [x] Refactor `render_json.c` to use cJSON for serialization
- [x] Remove hand-rolled parser functions
- [x] Update schema description (x86 → native)
- [x] Run existing simulator tests
- [ ] Add edge case tests for JSON parsing (deferred)
- [ ] Run valgrind to check for memory leaks (deferred)

**Results:**
- `command_handler.c`: 364 → 237 lines (removed ~170 lines of parser code)
- `render_json.c`: 181 → 138 lines (cleaner structure)
- All simulator tests pass
- JSON output verified working
