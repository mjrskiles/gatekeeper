#include "sim_hal.h"
#include "sim_state.h"
#include "input_source.h"
#include "cv_source.h"
#include "socket_server.h"
#include "command_handler.h"
#include "render/render.h"
#include "hardware/hal_interface.h"
#include "app_init.h"
#include "core/coordinator.h"
#include "output/led_feedback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * @file sim_main.c
 * @brief Gatekeeper x86 simulator entry point
 *
 * Headless architecture: state is collected into SimState,
 * then rendered by the selected renderer (terminal, JSON, batch).
 */

static volatile bool running = true;
static InputSource *input_source = NULL;
static Renderer *renderer = NULL;
static SocketServer *socket_server = NULL;
static SimState sim_state;
static LEDFeedbackController led_ctrl;
static CVSource cv_source;

static void handle_signal(int sig) {
    (void)sig;
    running = false;
}

/**
 * Get the CV source for external control (e.g., keyboard input).
 */
CVSource* sim_get_cv_source(void) {
    return &cv_source;
}

// Auto-release duration for tap keys (milliseconds) - for help text
#define TAP_AUTO_RELEASE_MS 200

static void print_usage(const char *progname) {
    printf("Gatekeeper Native Simulator\n\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  --script <file>  Run script file instead of interactive mode\n");
    printf("  --batch          Batch mode: plain text output (for CI/scripts)\n");
    printf("  --json           JSON output: one object per state change\n");
    printf("  --json-stream    JSON stream: continuous output at fixed interval\n");
    printf("  --socket [path]  Enable socket server (default: %s)\n", SOCKET_DEFAULT_PATH);
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Interactive Controls:\n");
    printf("  a          Tap Button A (auto-release after %dms)\n", TAP_AUTO_RELEASE_MS);
    printf("  b          Tap Button B (auto-release after %dms)\n", TAP_AUTO_RELEASE_MS);
    printf("  A          Hold Button A (until 'a' pressed to release)\n");
    printf("  B          Hold Button B (until 'b' pressed to release)\n");
    printf("  c/C        Toggle CV input (0V <-> 5V)\n");
    printf("  +/-        Adjust CV voltage (+/- 0.2V)\n");
    printf("  l          Cycle LFO (off -> 1Hz sine -> 2Hz tri -> 4Hz square -> off)\n");
    printf("  R          Reset time\n");
    printf("  L          Toggle legend\n");
    printf("  Q / ESC    Quit\n");
    printf("\n");
    printf("Script Format:\n");
    printf("  # Comment\n");
    printf("  <delay_ms> <action> [target] [value]\n");
    printf("  @<abs_ms>  <action> [target] [value]   (@ = absolute time)\n");
    printf("\n");
    printf("Actions: press, release, assert, log, quit\n");
    printf("Targets: a, b, cv, output\n");
    printf("\n");
    printf("Socket Protocol (NDJSON):\n");
    printf("  {\"cmd\": \"button\", \"id\": \"a\", \"state\": true}\n");
    printf("  {\"cmd\": \"cv_manual\", \"value\": 180}\n");
    printf("  {\"cmd\": \"cv_lfo\", \"freq_hz\": 2.0, \"shape\": \"sine\"}\n");
    printf("  {\"cmd\": \"cv_envelope\", \"attack_ms\": 10, \"decay_ms\": 100, \"sustain\": 180, \"release_ms\": 200}\n");
    printf("  {\"cmd\": \"cv_gate\", \"state\": true}\n");
    printf("  {\"cmd\": \"cv_trigger\"}\n");
    printf("  {\"cmd\": \"reset\"}\n");
    printf("  {\"cmd\": \"quit\"}\n");
    printf("\n");
}

// Track input state changes (sim-specific observation)
static void track_input_changes(void) {
    static bool last_button_a = false;
    static bool last_button_b = false;
    static uint8_t last_cv_voltage = 0;

    bool button_a = sim_get_button_a();
    bool button_b = sim_get_button_b();
    uint8_t cv_voltage = sim_get_cv_voltage();

    if (button_a != last_button_a) {
        sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
            "Button A %s", button_a ? "pressed" : "released");
        last_button_a = button_a;
    }

    if (button_b != last_button_b) {
        sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
            "Button B %s", button_b ? "pressed" : "released");
        last_button_b = button_b;
    }

    // Only log CV changes when they cross thresholds (avoid noise from +/-)
    // Log when voltage changes significantly (> ~0.5V)
    if ((cv_voltage > last_cv_voltage + 25) || (cv_voltage + 25 < last_cv_voltage)) {
        uint16_t mv = (uint16_t)cv_voltage * 5000 / 255;
        sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
            "CV -> %u.%uV", mv / 1000, (mv % 1000) / 100);
        last_cv_voltage = cv_voltage;
    }
}

// Update state tracking for render/logging (sim-specific observation)
static void track_state_changes(Coordinator *coord) {
    static TopState last_top_state = TOP_PERFORM;
    static ModeState last_mode = MODE_GATE;
    static MenuPage last_page = PAGE_GATE_CV;
    static bool last_output = false;

    TopState top_state = coordinator_get_top_state(coord);
    ModeState mode = coordinator_get_mode(coord);
    MenuPage page = coordinator_get_page(coord);
    bool in_menu = coordinator_in_menu(coord);
    bool output = coordinator_get_output(coord);

    // Log state changes (sim-specific event logging)
    if (top_state != last_top_state) {
        sim_state_add_event(&sim_state, EVT_TYPE_STATE_CHANGE, sim_get_time(),
            "State -> %s", sim_top_state_str(top_state));
        last_top_state = top_state;
    }

    if (mode != last_mode) {
        sim_state_add_event(&sim_state, EVT_TYPE_MODE_CHANGE, sim_get_time(),
            "Mode -> %s", sim_mode_str(mode));
        last_mode = mode;
    }

    if (in_menu && page != last_page) {
        sim_state_add_event(&sim_state, EVT_TYPE_PAGE_CHANGE, sim_get_time(),
            "Page -> %s", sim_page_str(page));
        last_page = page;
    }

    if (output != last_output) {
        sim_state_add_event(&sim_state, EVT_TYPE_OUTPUT, sim_get_time(),
            "Output -> %s", output ? "HIGH" : "LOW");
        last_output = output;
    }

    // Update state struct for renderer
    sim_state_set_fsm(&sim_state, top_state, mode, page, in_menu);
    sim_state_set_output(&sim_state, output);
}

int main(int argc, char **argv) {
    bool batch_mode = false;
    bool json_mode = false;
    bool json_stream = false;
    bool socket_mode = false;
    const char *socket_path = NULL;
    const char *script_file = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--json-stream") == 0) {
            json_mode = true;
            json_stream = true;
        } else if (strcmp(argv[i], "--script") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --script requires a filename\n");
                return 1;
            }
            script_file = argv[++i];
        } else if (strcmp(argv[i], "--socket") == 0) {
            socket_mode = true;
            // Optional path argument
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                socket_path = argv[++i];
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers for clean exit
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize state early so keyboard input can access it
    sim_state_init(&sim_state);

    // Create input source
    if (script_file) {
        input_source = input_source_script_create(script_file);
        if (!input_source) {
            return 1;  // Error already printed
        }
    } else {
        input_source = input_source_keyboard_create(&sim_state);
        if (!input_source) {
            fprintf(stderr, "Error: Failed to create keyboard input\n");
            return 1;
        }
    }

    // Create renderer based on mode
    if (json_mode) {
        renderer = render_json_create(json_stream);
    } else if (batch_mode) {
        renderer = render_batch_create();
    } else {
        renderer = render_terminal_create();
    }

    if (!renderer) {
        fprintf(stderr, "Error: Failed to create renderer\n");
        input_source->cleanup(input_source);
        return 1;
    }

    // Initialize renderer
    renderer->init(renderer);

    // Point global HAL to simulator HAL
    p_hal = sim_get_hal();

    // Initialize hardware (via sim HAL)
    p_hal->init();

    // Initialize CV source (starts in manual mode at 0V)
    cv_source_init(&cv_source);

    // Create socket server if requested
    if (socket_mode) {
        socket_server = socket_server_create(socket_path);
        if (!socket_server) {
            fprintf(stderr, "Error: Failed to create socket server\n");
            if (renderer) {
                renderer->cleanup(renderer);
                render_destroy(renderer);
            }
            if (input_source) {
                input_source->cleanup(input_source);
            }
            return 1;
        }
    }

    // Run app initialization
    AppSettings settings;
    AppInitResult init_result = app_init_run(&settings);

    if (init_result == APP_INIT_OK_FACTORY_RESET) {
        sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(), "Factory reset performed");
    } else if (init_result == APP_INIT_OK_DEFAULTS) {
        sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(), "Using default settings");
    }

    // Initialize coordinator
    Coordinator coordinator;
    coordinator_init(&coordinator, &settings);

    // Restore mode from saved settings
    if (settings.mode < MODE_COUNT) {
        coordinator_set_mode(&coordinator, (ModeState)settings.mode);
    }

    // Start coordinator
    coordinator_start(&coordinator);

    // Initialize LED feedback controller
    led_feedback_init(&led_ctrl);
    led_feedback_set_mode(&led_ctrl, coordinator_get_mode(&coordinator));

    sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(),
        "App initialized, mode=%s", sim_mode_str(coordinator_get_mode(&coordinator)));

    // Enable watchdog timer (250ms timeout) after init complete
    // This mirrors main.c - watchdog must be enabled AFTER app_init completes
    p_hal->wdt_enable();

    // Main loop
    uint32_t last_render = 0;
    while (running) {
        // Feed watchdog at start of each loop iteration (mirrors main.c)
        p_hal->wdt_reset();

        uint32_t current_time = p_hal->millis();

        // ========== SIM-SPECIFIC: Input handling ==========
        if (!input_source->update(input_source, current_time)) {
            break;
        }

        // Process socket commands (non-blocking)
        if (socket_server) {
            char cmd_buf[512];
            while (socket_server_poll(socket_server, cmd_buf, sizeof(cmd_buf))) {
                CommandResult result = command_handler_execute(cmd_buf, &cv_source);
                if (result.should_quit) {
                    running = false;
                    break;
                }
                if (!result.success && result.error[0]) {
                    fprintf(stderr, "Socket command error: %s\n", result.error);
                }
            }
        }

        // Update CV source and apply to simulated ADC
        uint8_t cv_val = cv_source_tick(&cv_source, 1);  // 1ms tick
        sim_set_cv_voltage(cv_val);

        // Track input changes for event logging
        track_input_changes();

        // ========== MIRRORS main.c: Application logic ==========
        // Update coordinator (processes inputs, runs mode handlers)
        coordinator_update(&coordinator);

        // Update LED feedback
        LEDFeedback feedback;
        coordinator_get_led_feedback(&coordinator, &feedback);
        led_feedback_update(&led_ctrl, &feedback, current_time);

        // Update output pin based on coordinator output state
        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }

        // ========== SIM-SPECIFIC: State observation & rendering ==========
        track_state_changes(&coordinator);

        // Update sim_state for renderer
        bool cv_digital = coordinator_get_cv_state(&coordinator);
        sim_state_set_inputs(&sim_state,
            sim_get_button_a(),
            sim_get_button_b(),
            cv_digital,
            sim_get_cv_voltage());

        for (int i = 0; i < SIM_NUM_LEDS; i++) {
            uint8_t r, g, b;
            sim_get_led(i, &r, &g, &b);
            sim_state_set_led(&sim_state, i, r, g, b);
        }

        sim_state_set_time(&sim_state, current_time);

        // Advance simulated time
        p_hal->advance_time(1);

        // Real-time pacing for interactive mode (keyboard input)
        bool realtime = input_source->is_realtime(input_source);
        if (realtime) {
            usleep(1000);  // 1ms
        }

        // Render periodically or on state change
        uint32_t now = p_hal->millis();
        uint32_t render_interval = realtime ? 100 : 500;
        if (sim_state_is_dirty(&sim_state) || (now - last_render >= render_interval)) {
            renderer->render(renderer, &sim_state);
            sim_state_clear_dirty(&sim_state);
            last_render = now;
        }

        // Send state to socket client (max 60Hz, only when dirty or connected)
        static uint32_t last_socket_send = 0;
        if (socket_server && socket_server_connected(socket_server)) {
            if ((now - last_socket_send) >= 16) {  // ~60Hz
                // Use JSON renderer to format state
                // For now, send a simple status line
                char state_json[512];
                snprintf(state_json, sizeof(state_json),
                    "{\"timestamp_ms\":%lu,\"state\":\"%s\",\"mode\":\"%s\","
                    "\"cv_voltage\":%u,\"output\":%s}",
                    (unsigned long)now,
                    sim_top_state_str(sim_state.top_state),
                    sim_mode_str(sim_state.mode),
                    sim_state.cv_voltage,
                    sim_state.signal_out ? "true" : "false");
                socket_server_send(socket_server, state_json);
                last_socket_send = now;
            }
        }
    }

    // Get result before cleanup
    bool failed = input_source->has_failed(input_source);

    // Cleanup
    if (socket_server) {
        socket_server_destroy(socket_server);
    }
    renderer->cleanup(renderer);
    render_destroy(renderer);
    input_source->cleanup(input_source);

    // Return non-zero exit code if any assertions failed
    return failed ? 1 : 0;
}
