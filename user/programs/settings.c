#include "user_lib.h"

#define MAPLE_MONO_8X8_SYMBOL user_gui_font
#include "maple_mono_8x8.h"
#undef MAPLE_MONO_8X8_SYMBOL

#include "../../kernel/apps/user_settings.c"

static user_settings_state_t app_state;
static size_t app_surface_capacity_pixels;

static int app_resize_surface(int window_id, user_settings_state_t* state) {
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
    if (pixels > app_surface_capacity_pixels || !state->surface) {
        surface = (uint32_t*)user_malloc(pixels * sizeof(uint32_t));
        if (!surface) return -1;
        if (state->surface) user_free(state->surface);
        state->surface = surface;
        app_surface_capacity_pixels = pixels;
    }
    state->render_w = new_w;
    state->render_h = new_h;
    return 0;
}

static void app_queue_event(user_settings_state_t* state, int type, int value) {
    int next_tail;

    if (!state) return;
    next_tail = (state->event_tail + 1) % USER_GUI_EVENT_QUEUE_CAP;
    if (next_tail == state->event_head) return;
    state->event_type[state->event_tail] = (uint16_t)type;
    state->event_arg[state->event_tail] = value;
    state->event_tail = next_tail;
}

static int app_pack_point(int x, int y) {
    return USER_SETTINGS_PACK_POINT(x, y);
}

static int app_present(int window_id, user_settings_state_t* state) {
    gui_present_params_t present;

    if (!state || !state->surface || state->render_w <= 0 || state->render_h <= 0) return -1;
    present.size = sizeof(present);
    present.flags = 0;
    present.buffer_ptr = (uintptr_t)state->surface;
    present.x = 0;
    present.y = 0;
    present.width = (uint32_t)state->render_w;
    present.height = (uint32_t)state->render_h;
    present.stride_bytes = (uint32_t)state->render_w * 4U;
    return user_gui_present(window_id, &present);
}

int main(void) {
    gui_create_window_params_t params;
    int window_id;
    int running = 1;
    uint32_t last_refresh = 0;

    app_state.render_w = USER_SETTINGS_SURFACE_W;
    app_state.render_h = USER_SETTINGS_SURFACE_H;

    params.size = sizeof(params);
    params.flags = 0;
    params.x = -1;
    params.y = -1;
    params.width = USER_SETTINGS_SURFACE_W + 2;
    params.height = USER_SETTINGS_SURFACE_H + 48;
    window_id = user_gui_create_window(&params);
    if (window_id < 0) return 1;
    (void)user_gui_set_title(window_id, "Settings");
    if (app_resize_surface(window_id, &app_state) != 0) {
        (void)user_gui_destroy_window(window_id);
        return 1;
    }
    app_state.dirty = 1;

    while (running) {
        gui_window_event_t event;
        int event_type;
        int event_value;
        uint32_t refresh = user_uptime_ticks() / 100U;
        int needs_render = app_state.dirty != 0 || refresh != last_refresh;

        while (user_gui_poll_event(window_id, &event) > 0) {
            if (event.type == GUI_WIN_EVT_CLOSE_REQUEST) {
                running = 0;
                break;
            }
            if (event.type == GUI_WIN_EVT_WINDOW_RESIZED) {
                (void)app_resize_surface(window_id, &app_state);
                app_state.dirty = 1;
                needs_render = 1;
            } else if (event.type == GUI_WIN_EVT_MOUSE_DOWN) {
                app_queue_event(&app_state, USER_SETTINGS_EVT_POINTER_DOWN,
                                app_pack_point(event.arg0, event.arg1));
            } else if (event.type == GUI_WIN_EVT_KEY_DOWN) {
                app_queue_event(&app_state, USER_SETTINGS_EVT_KEY_DOWN, event.arg0);
            } else if (event.type == GUI_WIN_EVT_PAINT ||
                       event.type == GUI_WIN_EVT_FOCUS_GAINED ||
                       event.type == GUI_WIN_EVT_FOCUS_LOST) {
                app_state.dirty = 1;
                needs_render = 1;
            }
        }

        while (settings_dequeue_event(&app_state, &event_type, &event_value)) {
            switch (event_type) {
                case USER_SETTINGS_EVT_ADJUST_OFFSET:
                    (void)user_set_timezone_offset_minutes(user_get_timezone_offset_minutes() + event_value);
                    (void)user_save_timezone_setting();
                    break;
                case USER_SETTINGS_EVT_SET_OFFSET:
                    (void)user_set_timezone_offset_minutes(event_value);
                    (void)user_save_timezone_setting();
                    break;
                case USER_SETTINGS_EVT_OPEN_CONFIG:
                    (void)user_save_timezone_setting();
                    break;
                case USER_SETTINGS_EVT_POINTER_DOWN:
                    settings_apply_hit(settings_hit_test(&app_state,
                                                         USER_SETTINGS_POINT_X(event_value),
                                                         USER_SETTINGS_POINT_Y(event_value)));
                    break;
                case USER_SETTINGS_EVT_KEY_DOWN:
                    settings_handle_key(event_value);
                    break;
                default:
                    break;
            }
            app_state.dirty = 1;
            needs_render = 1;
        }

        if (needs_render) {
            settings_render(&app_state);
            app_state.dirty = 0;
            last_refresh = refresh;
            (void)app_present(window_id, &app_state);
        }
        user_sleep(1);
    }

    (void)user_gui_destroy_window(window_id);
    if (app_state.surface) user_free(app_state.surface);
    app_surface_capacity_pixels = 0;
    return 0;
}
