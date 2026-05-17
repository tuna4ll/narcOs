#include "user_lib.h"

#include "../../kernel/apps/user_snake_app.c"

static user_snake_state_t app_state;
static size_t app_surface_capacity_pixels;

static int app_resize_surface(int window_id, user_snake_state_t* state) {
    gui_window_info_t info;
    int new_w;
    int new_h;
    size_t pixels;
    uint32_t* surface;

    if (!state || user_gui_get_window_info(window_id, &info) != 0) return -1;
    new_w = info.client_width;
    new_h = info.client_height;
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    pixels = (size_t)new_w * (size_t)new_h;
    if (pixels > app_surface_capacity_pixels || !state->surface_ptr) {
        surface = (uint32_t*)user_malloc(pixels * sizeof(uint32_t));
        if (!surface) return -1;
        if (state->surface_ptr) user_free(state->surface_ptr);
        state->surface_ptr = surface;
        app_surface_capacity_pixels = pixels;
    }
    state->render_w = new_w;
    state->render_h = new_h;
    return 0;
}

static int app_present(int window_id, user_snake_state_t* state) {
    gui_present_params_t present;

    if (!state || !state->surface_ptr || state->render_w <= 0 || state->render_h <= 0) return -1;
    present.size = sizeof(present);
    present.flags = 0;
    present.buffer_ptr = (uintptr_t)state->surface_ptr;
    present.x = 0;
    present.y = 0;
    present.width = (uint32_t)state->render_w;
    present.height = (uint32_t)state->render_h;
    present.stride_bytes = (uint32_t)state->render_w * 4U;
    return user_gui_present(window_id, &present);
}

static int app_key_to_dir(int scancode) {
    switch (scancode) {
        case 0x11:
        case 0x48: return 0;
        case 0x1F:
        case 0x50: return 1;
        case 0x1E:
        case 0x4B: return 2;
        case 0x20:
        case 0x4D: return 3;
        default: return -1;
    }
}

int main(void) {
    gui_create_window_params_t params;
    int window_id;
    int running = 1;
    int needs_render = 1;

    params.size = sizeof(params);
    params.flags = 0;
    params.x = -1;
    params.y = -1;
    params.width = USER_SNAKE_SURFACE_W + 2;
    params.height = USER_SNAKE_SURFACE_H + UI_WINDOW_CLIENT_TOP + 8;
    window_id = user_gui_create_window(&params);
    if (window_id < 0) return 1;
    (void)user_gui_set_title(window_id, "Snake");
    if (app_resize_surface(window_id, &app_state) != 0) {
        (void)user_gui_destroy_window(window_id);
        return 1;
    }
    snake_reset(&app_state);

    while (running) {
        gui_window_event_t event;

        while (user_gui_poll_event(window_id, &event) > 0) {
            if (event.type == GUI_WIN_EVT_CLOSE_REQUEST) {
                running = 0;
                break;
            }
            if (event.type == GUI_WIN_EVT_KEY_DOWN) {
                int dir = app_key_to_dir(event.arg0);
                if (dir >= 0 && !snake_direction_is_reverse(app_state.dir, dir)) {
                    app_state.dir = dir;
                    needs_render = 1;
                } else if (event.arg0 == 0x13 || event.arg0 == 0x19) {
                    snake_reset(&app_state);
                    needs_render = 1;
                } else if (event.arg0 == 0x10 || event.arg0 == 0x01) {
                    running = 0;
                    break;
                }
            } else if (event.type == GUI_WIN_EVT_WINDOW_RESIZED) {
                (void)app_resize_surface(window_id, &app_state);
                needs_render = 1;
            } else if (event.type == GUI_WIN_EVT_PAINT ||
                       event.type == GUI_WIN_EVT_FOCUS_GAINED ||
                       event.type == GUI_WIN_EVT_FOCUS_LOST) {
                needs_render = 1;
            }
        }

        if (!app_state.dead) {
            uint32_t now = user_uptime_ticks();

            if ((uint32_t)(now - (uint32_t)app_state.last_tick) > 10U) {
                app_state.last_tick = (int)now;
                snake_step(&app_state);
                needs_render = 1;
            }
        }

        if (needs_render) {
            snake_render(&app_state);
            (void)app_present(window_id, &app_state);
            needs_render = 0;
        }
        user_sleep(1);
    }

    (void)user_gui_destroy_window(window_id);
    if (app_state.surface_ptr) user_free(app_state.surface_ptr);
    app_surface_capacity_pixels = 0;
    return 0;
}
