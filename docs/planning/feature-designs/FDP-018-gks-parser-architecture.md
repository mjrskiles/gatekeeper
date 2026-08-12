---
type: fdp
id: FDP-018
status: proposed
created: 2025-12-24
modified: 2025-12-24
supersedes: null
superseded_by: null
obsoleted_by: null
related: [AP-008, FDP-017]
---

# FDP-018: GKS Parser Architecture

## Status

Proposed

## Summary

Design a C++ parser architecture for GKS v1.5 consisting of four stages: Lexer, Parser, Resolver, and Executor. The system uses a tree-walking interpreter with an AST, builds an event timeline for absolute time scheduling, and supports a flat module system for includes.

## Motivation

FDP-017 defines the GKS v1.5 language specification. This FDP defines how to implement it:

- **Lexer**: Tokenize source with line structure preserved
- **Parser**: Build AST via recursive descent
- **Resolver**: Build symbol tables, expand timers, create timeline
- **Executor**: Walk timeline, drive simulator HAL

### Design Goals

- **Clear separation**: Each stage has single responsibility
- **Excellent errors**: Line/column, context, suggestions
- **Testable**: Each stage independently testable
- **Simple**: No bytecode, no optimization passes

## Detailed Design

### Architecture Overview

```
┌──────────┐    ┌────────┐    ┌─────────┐    ┌──────────┐    ┌──────────┐
│  Source  │───▶│ Lexer  │───▶│ Parser  │───▶│ Resolver │───▶│ Executor │
│  Files   │    │        │    │         │    │          │    │          │
└──────────┘    └────────┘    └─────────┘    └──────────┘    └──────────┘
                    │              │              │               │
                 Tokens          AST          Timeline         HAL calls
                 + EOL                       + Symbols
```

### Stage 1: Lexer

The lexer converts source text into a stream of tokens, preserving line structure.

#### Token Types

```cpp
enum class TokenType {
    // Literals
    INTEGER,        // 123, 0xFF, 0b1010
    VOLTAGE,        // 2.5v
    STRING,         // "hello"
    IDENTIFIER,     // press_a, buttons

    // Keywords
    CONST, DEF, END, TIMER, COUNT,
    SET, ASSERT, WAIT, LOG, QUIT, FAIL, BREAK,
    INCLUDE, TRIGGER, FAULT, INSPECT,
    HIGH, LOW, PIN, ADC,

    // Punctuation
    PLUS, MINUS, STAR, SLASH, PERCENT,  // + - * / %
    AT,                                  // @
    DOT,                                 // .
    EQUALS,                              // =
    LPAREN, RPAREN,                      // ( )

    // Structure
    EOL,            // newline (significant)
    END_OF_FILE,

    // Error
    ERROR,          // lexer error token
};

struct Token {
    TokenType type;
    std::string lexeme;
    SourceLoc loc;

    // For INTEGER tokens, pre-parsed value
    std::optional<int32_t> int_value;
};

struct SourceLoc {
    std::string file;
    int line;
    int col;
};
```

#### Lexer Behavior

1. **Whitespace**: Skip spaces and tabs between tokens (not significant)
2. **Newlines**: Emit `EOL` token (line structure matters)
3. **Comments**: `#` to end of line, stripped entirely
4. **Keywords**: Case-insensitive (`SET`, `Set`, `set` all valid)
5. **Identifiers**: Case-sensitive, start with letter, contain `[a-zA-Z0-9_]`
6. **Integers**: Decimal, hex (`0x`), binary (`0b`)
7. **Voltage**: Decimal followed by `v` (e.g., `2.5v`)
8. **Strings**: Double-quoted, supports `\"` and `\\` escapes

#### Lexer Interface

```cpp
class Lexer {
public:
    explicit Lexer(std::string source, std::string filename);

    Token next_token();
    Token peek_token();
    bool at_end() const;

    // For error reporting
    std::string get_line(int line_number) const;

private:
    std::string source_;
    std::string filename_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    char peek() const;
    char advance();
    void skip_whitespace();
    void skip_comment();
    Token scan_token();
    Token scan_identifier();
    Token scan_number();
    Token scan_string();
};
```

#### Error Tokens

Lexer errors produce `ERROR` tokens rather than throwing:

```cpp
Token scan_string() {
    // ...
    if (at_end() || peek() == '\n') {
        return make_error("unterminated string");
    }
    // ...
}
```

Parser checks for error tokens and reports accumulated errors.

### Stage 2: Parser

Recursive descent parser producing an AST. Grammar is LL(1) - single token lookahead suffices.

#### AST Node Types

```cpp
// Forward declarations
struct Script;
struct ConstDecl;
struct Definition;
struct TimerDef;
struct Command;

// Root node
struct Script {
    std::string filename;
    std::vector<std::unique_ptr<Statement>> statements;
};

// Statement variants
using Statement = std::variant<
    ConstDecl,
    Definition,
    TimerDef,
    IncludeStmt,
    Command
>;

// const NAME = expr
struct ConstDecl {
    std::string name;
    std::unique_ptr<ConstExpr> value;
    SourceLoc loc;
};

// def NAME ... end
struct Definition {
    std::string name;
    std::vector<Command> body;
    SourceLoc loc;
};

// timer NAME interval [count N] ... end
struct TimerDef {
    std::string name;
    int32_t interval_ms;
    std::optional<int32_t> count;  // nullopt = must use start/stop
    std::vector<Command> body;
    SourceLoc loc;
};

// include "path"
struct IncludeStmt {
    std::string path;
    SourceLoc loc;
};

// [time_prefix] action
struct Command {
    std::optional<TimePrefix> time;
    std::unique_ptr<Action> action;
    SourceLoc loc;
};

// Time prefix: @N (absolute), +N (relative), N (relative shorthand)
struct TimePrefix {
    enum Kind { ABSOLUTE, RELATIVE };
    Kind kind;
    int32_t value_ms;
};

// Action variants
using Action = std::variant<
    SetCmd,
    AssertCmd,
    WaitCmd,
    LogCmd,
    QuitCmd,
    FailCmd,
    BreakCmd,
    TriggerCmd,
    FaultCmd,
    InspectCmd,
    UserCall       // call to def or timer
>;

struct SetCmd {
    HWTarget target;
    Value value;
};

struct AssertCmd {
    HWTarget target;
    Value value;
};

struct WaitCmd {
    ValueOrConst duration;
};

struct LogCmd {
    std::string message;
};

struct QuitCmd {};

struct FailCmd {
    std::optional<std::string> message;
};

struct BreakCmd {};

struct TriggerCmd {
    TriggerSource source;  // wdt, reset, int.X
};

struct FaultCmd {
    Subsystem subsystem;   // adc, eeprom, timer
    FaultMode mode;        // normal, timeout, stuck_low, etc.
};

struct InspectCmd {
    InspectTarget target;  // state, memory, register, hw_target
};

struct UserCall {
    std::optional<std::string> module;  // nullopt = local, "buttons" = qualified
    std::string name;
};

// Hardware targets
struct HWTarget {
    enum Kind { PIN, ADC };
    Kind kind;
    std::string name;  // "a", "b", "output", "cv", "0", etc.
};

// Values
struct Value {
    enum Kind { PIN_STATE, INTEGER, VOLTAGE, CONST_REF };
    Kind kind;
    std::variant<bool, int32_t, double, std::string> data;
};

// Const expressions (evaluated at resolve time)
using ConstExpr = std::variant<
    int32_t,                    // literal
    double,                     // voltage literal (converted to int)
    std::string,                // const reference
    std::unique_ptr<BinaryExpr>,
    std::unique_ptr<UnaryExpr>
>;

struct BinaryExpr {
    enum Op { ADD, SUB, MUL, DIV, MOD };
    Op op;
    ConstExpr left;
    ConstExpr right;
};

struct UnaryExpr {
    enum Op { NEG };
    Op op;
    ConstExpr operand;
};
```

#### Parser Interface

```cpp
class Parser {
public:
    explicit Parser(Lexer lexer);

    // Returns Script or throws ParseError
    Script parse();

    // Accumulated errors (if not throwing)
    const std::vector<ParseError>& errors() const;

private:
    Lexer lexer_;
    Token current_;
    Token previous_;
    std::vector<ParseError> errors_;

    // Helpers
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    void synchronize();  // error recovery to next line

    // Grammar rules
    Script parse_script();
    Statement parse_statement();
    ConstDecl parse_const_decl();
    Definition parse_definition();
    TimerDef parse_timer_def();
    IncludeStmt parse_include();
    Command parse_command();
    std::optional<TimePrefix> parse_time_prefix();
    Action parse_action();
    ConstExpr parse_const_expr();
    // ... etc
};
```

#### Parsing Strategy

Each grammar rule maps to a method:

```cpp
// command = [ time_prefix ] action ;
Command Parser::parse_command() {
    Command cmd;
    cmd.loc = current_.loc;
    cmd.time = parse_time_prefix();
    cmd.action = parse_action();
    return cmd;
}

// time_prefix = '@' integer | '+' integer | integer ;
std::optional<TimePrefix> Parser::parse_time_prefix() {
    if (match(TokenType::AT)) {
        auto val = consume(TokenType::INTEGER, "expected time after '@'");
        return TimePrefix{TimePrefix::ABSOLUTE, *val.int_value};
    }
    if (match(TokenType::PLUS)) {
        auto val = consume(TokenType::INTEGER, "expected time after '+'");
        return TimePrefix{TimePrefix::RELATIVE, *val.int_value};
    }
    if (check(TokenType::INTEGER)) {
        // Lookahead: is this a time prefix or a value?
        // If followed by action keyword, it's a time prefix
        if (is_action_start(peek_next())) {
            auto val = advance();
            return TimePrefix{TimePrefix::RELATIVE, *val.int_value};
        }
    }
    return std::nullopt;
}
```

### Stage 3: Resolver

The resolver performs semantic analysis and builds execution structures.

#### Responsibilities

1. **Process includes**: Load and parse included files, build module registry
2. **Build symbol tables**: Collect all consts, defs, timers
3. **Evaluate const expressions**: Compute values at resolve time
4. **Expand timers**: Convert finite timers to scheduled events
5. **Build timeline**: Sort all commands by absolute time
6. **Validate references**: Check all names resolve, detect cycles

#### Data Structures

```cpp
// Module = one source file's exports
struct Module {
    std::string name;           // "buttons" (derived from filename)
    std::string path;           // "lib/buttons.gks" (for errors)

    std::unordered_map<std::string, int32_t> consts;
    std::unordered_map<std::string, Definition*> defs;
    std::unordered_map<std::string, TimerDef*> timers;
};

// Scheduled event for timeline
struct ScheduledEvent {
    int32_t time_ms;
    int32_t source_order;       // for stable sort
    SourceLoc loc;              // for error reporting

    // What to execute
    std::variant<
        Action,                 // direct action
        UserCall,               // call to def
        TimerTick               // timer iteration
    > payload;
};

struct TimerTick {
    std::string timer_name;
    int32_t iteration;          // 0, 1, 2, ...
};

// Resolver output
struct ResolvedScript {
    std::vector<Module> modules;
    Module main_module;         // the entry script

    std::vector<ScheduledEvent> timeline;  // sorted by time

    // For infinite timers (need runtime handling)
    std::vector<TimerDef*> infinite_timers;
};
```

#### Module Loading

```cpp
class Resolver {
public:
    ResolvedScript resolve(Script& main_script);

private:
    std::unordered_map<std::string, Module> modules_;
    std::unordered_set<std::string> loading_;  // for cycle detection
    int32_t current_time_ = 0;
    int32_t source_order_ = 0;

    Module& load_module(const std::string& path, const SourceLoc& include_loc);
    std::string module_name_from_path(const std::string& path);
    void check_name_collision(const std::string& name, const std::string& path,
                              const SourceLoc& loc);
};

Module& Resolver::load_module(const std::string& path, const SourceLoc& include_loc) {
    std::string name = module_name_from_path(path);

    // Check for circular include
    if (loading_.contains(path)) {
        throw ResolveError(include_loc, "circular include detected",
                          format_include_chain());
    }

    // Check for name collision
    if (modules_.contains(name)) {
        throw ResolveError(include_loc,
            fmt::format("module name collision: '{}'", name),
            fmt::format("conflicts with '{}'", modules_[name].path));
    }

    // Parse the included file
    loading_.insert(path);
    auto source = read_file(path);
    Lexer lexer(source, path);
    Parser parser(lexer);
    Script script = parser.parse();

    // Recursively resolve includes in this file
    Module module;
    module.name = name;
    module.path = path;

    for (auto& stmt : script.statements) {
        if (auto* inc = std::get_if<IncludeStmt>(&stmt)) {
            load_module(inc->path, inc->loc);
        }
        // ... collect consts, defs, timers into module
    }

    loading_.erase(path);
    modules_[name] = std::move(module);
    return modules_[name];
}

std::string Resolver::module_name_from_path(const std::string& path) {
    // "lib/buttons.gks" → "buttons"
    auto filename = std::filesystem::path(path).stem().string();
    return filename;
}
```

#### Const Evaluation

```cpp
int32_t Resolver::evaluate_const(const ConstExpr& expr, const Module& ctx) {
    return std::visit(overloaded{
        [](int32_t lit) { return lit; },

        [](double voltage) {
            // 0-5V → 0-255
            return static_cast<int32_t>((voltage / 5.0) * 255.0);
        },

        [&](const std::string& name) {
            // Check local consts first
            if (ctx.consts.contains(name)) {
                return ctx.consts.at(name);
            }
            // Check if qualified (module.const)
            if (auto dot = name.find('.'); dot != std::string::npos) {
                auto mod = name.substr(0, dot);
                auto cname = name.substr(dot + 1);
                if (!modules_.contains(mod)) {
                    throw ResolveError("unknown module: " + mod);
                }
                if (!modules_[mod].consts.contains(cname)) {
                    throw ResolveError("unknown const: " + name);
                }
                return modules_[mod].consts.at(cname);
            }
            throw ResolveError("undefined constant: " + name);
        },

        [&](const std::unique_ptr<BinaryExpr>& bin) {
            auto l = evaluate_const(bin->left, ctx);
            auto r = evaluate_const(bin->right, ctx);
            switch (bin->op) {
                case BinaryExpr::ADD: return l + r;
                case BinaryExpr::SUB: return l - r;
                case BinaryExpr::MUL: return l * r;
                case BinaryExpr::DIV:
                    if (r == 0) throw ResolveError("division by zero");
                    return l / r;
                case BinaryExpr::MOD:
                    if (r == 0) throw ResolveError("modulo by zero");
                    return l % r;
            }
        },

        [&](const std::unique_ptr<UnaryExpr>& un) {
            auto v = evaluate_const(un->operand, ctx);
            if (un->op == UnaryExpr::NEG) return -v;
            return v;
        }
    }, expr);
}
```

#### Timeline Building

```cpp
void Resolver::build_timeline(Script& script, ResolvedScript& output) {
    current_time_ = 0;
    source_order_ = 0;

    for (auto& stmt : script.statements) {
        if (auto* cmd = std::get_if<Command>(&stmt)) {
            schedule_command(*cmd, output);
        }
        // Defs and timers are collected but not scheduled directly
        // UserCalls to them are scheduled
    }

    // Sort timeline by (time, source_order)
    std::stable_sort(output.timeline.begin(), output.timeline.end(),
        [](const auto& a, const auto& b) {
            if (a.time_ms != b.time_ms) return a.time_ms < b.time_ms;
            return a.source_order < b.source_order;
        });
}

void Resolver::schedule_command(const Command& cmd, ResolvedScript& output) {
    int32_t exec_time = current_time_;

    if (cmd.time) {
        if (cmd.time->kind == TimePrefix::ABSOLUTE) {
            exec_time = cmd.time->value_ms;
            if (exec_time < current_time_) {
                throw ResolveError(cmd.loc,
                    fmt::format("absolute time @{} is before current time {}",
                               exec_time, current_time_));
            }
        } else {
            exec_time = current_time_ + cmd.time->value_ms;
        }
    }

    // Handle wait specially - it advances current_time_
    if (auto* wait = std::get_if<WaitCmd>(&*cmd.action)) {
        int32_t duration = resolve_value(wait->duration);
        current_time_ = exec_time + duration;
        return;  // wait itself doesn't create an event
    }

    output.timeline.push_back(ScheduledEvent{
        .time_ms = exec_time,
        .source_order = source_order_++,
        .loc = cmd.loc,
        .payload = *cmd.action
    });
}
```

#### Timer Expansion

```cpp
void Resolver::expand_timer(const TimerDef& timer, int32_t start_time,
                           ResolvedScript& output) {
    if (!timer.count) {
        // Infinite timer - executor handles at runtime
        output.infinite_timers.push_back(&timer);
        return;
    }

    // Finite timer - fully expand at resolve time
    for (int32_t i = 0; i < *timer.count; ++i) {
        int32_t iter_start = start_time + (i * timer.interval_ms);

        // Schedule each command in body relative to iteration start
        int32_t body_time = 0;
        for (const auto& cmd : timer.body) {
            int32_t cmd_time = iter_start + body_time;

            if (cmd.time) {
                if (cmd.time->kind == TimePrefix::RELATIVE) {
                    cmd_time = iter_start + cmd.time->value_ms;
                }
                // ABSOLUTE within timer body? Error or relative to iter_start?
                // Let's make it relative to iteration start
            }

            output.timeline.push_back(ScheduledEvent{
                .time_ms = cmd_time,
                .source_order = source_order_++,
                .loc = cmd.loc,
                .payload = TimerTick{timer.name, i}
            });

            // Track body time for relative positioning
            if (auto* wait = std::get_if<WaitCmd>(&*cmd.action)) {
                body_time += resolve_value(wait->duration);
            }
        }
    }
}
```

### Stage 4: Executor

Walks the timeline and executes actions against the simulator HAL.

#### Interface

```cpp
class Executor {
public:
    Executor(ResolvedScript script, SimulatorHAL& hal);

    // Run until completion or error
    ExecutionResult run();

    // Step-by-step execution (for debugging)
    bool step();  // returns false when done

    // Current state
    int32_t current_time() const;
    bool is_running() const;

private:
    ResolvedScript script_;
    SimulatorHAL& hal_;

    size_t timeline_index_ = 0;
    int32_t current_time_ = 0;
    bool running_ = true;

    // Call stack for def expansion
    struct CallFrame {
        const Definition* def;
        size_t command_index;
        std::string module;
    };
    std::vector<CallFrame> call_stack_;

    // Active infinite timers
    struct ActiveTimer {
        const TimerDef* timer;
        int32_t next_fire_time;
    };
    std::vector<ActiveTimer> active_timers_;

    void execute_event(const ScheduledEvent& event);
    void execute_action(const Action& action);
    void execute_def(const Definition& def, const std::string& module);
    void execute_timer_tick(const TimerTick& tick);
};
```

#### Execution Loop

```cpp
ExecutionResult Executor::run() {
    while (running_ && timeline_index_ < script_.timeline.size()) {
        const auto& event = script_.timeline[timeline_index_];

        // Advance virtual time
        current_time_ = event.time_ms;
        hal_.set_time(current_time_);

        // Execute
        try {
            execute_event(event);
        } catch (const ExecutionError& e) {
            return ExecutionResult::error(e.what(), event.loc);
        }

        ++timeline_index_;

        // Check for infinite timer re-scheduling
        for (auto& timer : active_timers_) {
            if (timer.next_fire_time <= current_time_) {
                schedule_timer_iteration(timer);
            }
        }
    }

    return ExecutionResult::success();
}

void Executor::execute_event(const ScheduledEvent& event) {
    std::visit(overloaded{
        [&](const Action& action) { execute_action(action); },
        [&](const UserCall& call) { execute_user_call(call); },
        [&](const TimerTick& tick) { execute_timer_tick(tick); }
    }, event.payload);
}

void Executor::execute_action(const Action& action) {
    std::visit(overloaded{
        [&](const SetCmd& cmd) {
            if (cmd.target.kind == HWTarget::PIN) {
                hal_.set_pin(cmd.target.name, resolve_pin_state(cmd.value));
            } else {
                hal_.set_adc(cmd.target.name, resolve_int_value(cmd.value));
            }
        },

        [&](const AssertCmd& cmd) {
            auto expected = resolve_value(cmd.value);
            auto actual = cmd.target.kind == HWTarget::PIN
                ? hal_.read_pin(cmd.target.name)
                : hal_.read_adc(cmd.target.name);
            if (expected != actual) {
                throw AssertionError(cmd, expected, actual);
            }
        },

        [&](const WaitCmd&) {
            // Wait is handled in timeline building, no runtime action
        },

        [&](const LogCmd& cmd) {
            hal_.log(current_time_, cmd.message);
        },

        [&](const QuitCmd&) {
            running_ = false;
        },

        [&](const FailCmd& cmd) {
            throw FailError(cmd.message.value_or("test failed"));
        },

        [&](const BreakCmd&) {
            // Pop call stack to exit current def
            if (!call_stack_.empty()) {
                call_stack_.pop_back();
            }
        },

        [&](const TriggerCmd& cmd) {
            hal_.trigger(cmd.source);
        },

        [&](const FaultCmd& cmd) {
            hal_.set_fault(cmd.subsystem, cmd.mode);
        },

        [&](const InspectCmd& cmd) {
            auto result = hal_.inspect(cmd.target);
            hal_.log(current_time_, result);
        },

        [&](const UserCall& call) {
            execute_user_call(call);
        }
    }, action);
}
```

### Error Reporting

All stages use consistent error formatting:

```cpp
struct CompileError {
    SourceLoc loc;
    std::string message;
    std::optional<std::string> note;
    std::optional<std::string> help;

    std::string format(const SourceProvider& sources) const;
};

std::string CompileError::format(const SourceProvider& sources) const {
    std::ostringstream out;

    // Header
    out << "error: " << message << "\n";

    // Location with source line
    out << "  --> " << loc.file << ":" << loc.line << ":" << loc.col << "\n";
    out << "    |\n";

    // Source line with underline
    std::string line = sources.get_line(loc.file, loc.line);
    out << fmt::format(" {:3} | {}\n", loc.line, line);
    out << "    | " << std::string(loc.col - 1, ' ')
        << "^" << std::string(std::max(0, (int)lexeme_.size() - 1), '~') << "\n";

    // Note
    if (note) {
        out << "    |\n";
        out << "  = note: " << *note << "\n";
    }

    // Help
    if (help) {
        out << "  = help: " << *help << "\n";
    }

    return out.str();
}
```

#### Fuzzy Matching for Suggestions

```cpp
std::optional<std::string> find_similar(const std::string& name,
                                        const std::vector<std::string>& candidates) {
    int best_distance = INT_MAX;
    std::string best_match;

    for (const auto& candidate : candidates) {
        int dist = levenshtein_distance(name, candidate);
        if (dist < best_distance && dist <= 3) {  // threshold
            best_distance = dist;
            best_match = candidate;
        }
    }

    if (best_distance <= 3) {
        return best_match;
    }
    return std::nullopt;
}

// Usage in resolver:
if (!modules_.contains(module_name)) {
    auto candidates = get_module_names();
    auto similar = find_similar(module_name, candidates);

    CompileError err;
    err.loc = loc;
    err.message = fmt::format("unknown module '{}'", module_name);
    err.note = fmt::format("available modules: {}", join(candidates, ", "));
    if (similar) {
        err.help = fmt::format("did you mean '{}'?", *similar);
    }
    throw err;
}
```

## File Structure

```
sim/gks/
├── lexer.hpp           # Token types, Lexer class
├── lexer.cpp
├── parser.hpp          # AST types, Parser class
├── parser.cpp
├── resolver.hpp        # Module, ResolvedScript, Resolver class
├── resolver.cpp
├── executor.hpp        # Executor class
├── executor.cpp
├── error.hpp           # CompileError, error formatting
├── error.cpp
├── ast.hpp             # AST node definitions (if separate from parser.hpp)
└── gks.hpp             # Public API: compile_and_run(path, hal)
```

## Public API

```cpp
// sim/gks/gks.hpp

namespace gks {

struct RunResult {
    bool success;
    int exit_code;               // 0 = success, 1 = fail/error
    std::optional<std::string> error_message;
    int32_t final_time_ms;
};

// Compile and run a script file
RunResult run_file(const std::string& path, SimulatorHAL& hal);

// Compile and run from string (for testing)
RunResult run_source(const std::string& source, const std::string& filename,
                     SimulatorHAL& hal);

// Just compile (for syntax checking)
std::variant<ResolvedScript, std::vector<CompileError>>
compile_file(const std::string& path);

}  // namespace gks
```

## Implementation Phases

### Phase 1: Lexer
- Token types and Token struct
- Lexer class with all scanning logic
- Unit tests for all token types
- Error token generation

### Phase 2: Parser
- AST node definitions
- Recursive descent parser
- Unit tests for all grammar rules
- Parse error reporting

### Phase 3: Resolver - Basic
- Const evaluation
- Def collection
- Basic timeline building (no includes, no timers)
- Unit tests

### Phase 4: Resolver - Includes
- Module loading
- Qualified name resolution
- Circular include detection
- Include path handling

### Phase 5: Resolver - Timers
- Timer def parsing
- Finite timer expansion
- Infinite timer tracking

### Phase 6: Executor
- Timeline execution
- HAL integration
- Def expansion (call stack)
- Assertion checking

### Phase 7: Integration
- Wire into simulator main
- Update existing .gks tests
- Delete old C parser

## Testing Strategy

Each stage independently testable:

```cpp
// Lexer tests
TEST(Lexer, IntegerLiterals) {
    Lexer lex("123 0xFF 0b1010", "test");
    EXPECT_EQ(lex.next_token().type, TokenType::INTEGER);
    EXPECT_EQ(lex.next_token().int_value, 0xFF);
    // ...
}

// Parser tests
TEST(Parser, ConstDecl) {
    auto script = parse("const FOO = 123");
    ASSERT_EQ(script.statements.size(), 1);
    auto* decl = std::get_if<ConstDecl>(&script.statements[0]);
    ASSERT_NE(decl, nullptr);
    EXPECT_EQ(decl->name, "FOO");
}

// Resolver tests
TEST(Resolver, ConstEvaluation) {
    auto resolved = resolve("const A = 10\nconst B = A * 2");
    EXPECT_EQ(resolved.main_module.consts["A"], 10);
    EXPECT_EQ(resolved.main_module.consts["B"], 20);
}

// Executor tests
TEST(Executor, SetPin) {
    MockHAL hal;
    run_source("set pin.a low", "test", hal);
    EXPECT_EQ(hal.pin_state("a"), false);
}
```

## Open Questions

1. **Include search paths**: Just relative to current file, or also check a `GKS_PATH`?

2. **Timer body time semantics**: Are times in timer body relative to iteration start, or cumulative?

3. **Break in timer**: Should `break` exit the current timer iteration or stop the timer entirely?

4. **Max recursion depth**: Should we limit def call depth to prevent stack overflow?

## References

- FDP-017: GKS Language Specification v1.5
- `sim/gks.ebnf`: Formal grammar
- AP-008: Parent action plan

---

## Addenda
