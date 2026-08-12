#include "render.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file render_json.c
 * @brief JSON renderer for simulator using cJSON
 *
 * Outputs simulator state as newline-delimited JSON (NDJSON).
 * Each state change produces one JSON object on stdout.
 */

typedef struct {
    bool stream_mode;
    int last_event_count;
} JsonCtx;

static void json_init(Renderer *self) {
    (void)self;
    // No terminal setup needed
}

static void json_render(Renderer *self, const SimState *state) {
    JsonCtx *ctx = (JsonCtx*)self->ctx;

    // Build JSON object
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

    // Events array (only new events since last render)
    cJSON *events = cJSON_AddArrayToObject(root, "events");
    int start = (state->event_count < SIM_MAX_EVENTS) ? 0 : state->event_head;
    int count = (state->event_count < SIM_MAX_EVENTS) ? state->event_count : SIM_MAX_EVENTS;

    // In stream mode, output all events; otherwise just new ones
    int events_to_output = ctx->stream_mode ? count : (state->event_count - ctx->last_event_count);
    if (events_to_output > count) events_to_output = count;
    if (events_to_output < 0) events_to_output = 0;

    int output_start = (start + count - events_to_output) % SIM_MAX_EVENTS;
    for (int i = 0; i < events_to_output; i++) {
        int idx = (output_start + i) % SIM_MAX_EVENTS;
        cJSON *event = cJSON_CreateObject();
        cJSON_AddNumberToObject(event, "time_ms", state->events[idx].time_ms);
        cJSON_AddStringToObject(event, "type", sim_event_type_str(state->events[idx].type));
        cJSON_AddStringToObject(event, "message", state->events[idx].message);
        cJSON_AddItemToArray(events, event);
    }

    // Print unformatted (compact) JSON and cleanup
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        printf("%s\n", json_str);
        fflush(stdout);
        free(json_str);
    }

    cJSON_Delete(root);

    ctx->last_event_count = state->event_count;
}

static bool json_handle_input(Renderer *self, SimState *state, int key) {
    (void)self;
    (void)state;
    // JSON mode doesn't handle keyboard input interactively
    // Quit on 'q' or ESC for consistency
    if (key == 'q' || key == 'Q' || key == 27) {
        return false;
    }
    return true;
}

static void json_cleanup(Renderer *self) {
    (void)self;
    // No cleanup needed
}

Renderer* render_json_create(bool stream_mode) {
    Renderer *r = malloc(sizeof(Renderer));
    if (!r) return NULL;

    JsonCtx *ctx = malloc(sizeof(JsonCtx));
    if (!ctx) {
        free(r);
        return NULL;
    }

    ctx->stream_mode = stream_mode;
    ctx->last_event_count = 0;

    r->init = json_init;
    r->render = json_render;
    r->handle_input = json_handle_input;
    r->cleanup = json_cleanup;
    r->ctx = ctx;

    return r;
}
