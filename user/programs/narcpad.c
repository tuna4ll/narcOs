#include "user_lib.h"

#define MAPLE_MONO_8X8_SYMBOL user_gui_font
#include "maple_mono_8x8.h"
#undef MAPLE_MONO_8X8_SYMBOL

#include "../../kernel/apps/user_narcpad.c"

static user_narcpad_state_t app_state;
static size_t app_surface_capacity_pixels;

static int app_resize_surface(int window_id, user_narcpad_state_t* state) {
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

static void app_copy_text(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0U) return;
    if (!src) src = "";
    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void app_queue_event(user_narcpad_state_t* state, int type, int value) {
    int next_tail;

    if (!state) return;
    next_tail = (state->event_tail + 1) % USER_GUI_EVENT_QUEUE_CAP;
    if (next_tail == state->event_head) return;
    state->event_type[state->event_tail] = (uint16_t)type;
    state->event_arg[state->event_tail] = value;
    state->event_tail = next_tail;
}

static int app_present(int window_id, user_narcpad_state_t* state) {
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

static void app_handle_gui_event(user_narcpad_state_t* state, const gui_window_event_t* event) {
    if (!state || !event) return;
    switch (event->type) {
        case GUI_WIN_EVT_KEY_DOWN:
            if ((event->arg2 & 2) != 0 && event->arg0 == 0x1F) {
                app_queue_event(state, USER_NARCPAD_EVT_SAVE, 0);
            }
            break;
        case GUI_WIN_EVT_CHAR:
            if (event->arg0 == '\b') app_queue_event(state, USER_NARCPAD_EVT_BACKSPACE, 0);
            else if (event->arg0 == '\n') app_queue_event(state, USER_NARCPAD_EVT_NEWLINE, 0);
            else if (event->arg0 != 0) app_queue_event(state, USER_NARCPAD_EVT_CHAR, event->arg0);
            break;
        case GUI_WIN_EVT_MOUSE_WHEEL:
            app_queue_event(state, USER_NARCPAD_EVT_SCROLL, event->arg2);
            break;
        case GUI_WIN_EVT_PAINT:
        case GUI_WIN_EVT_FOCUS_GAINED:
        case GUI_WIN_EVT_FOCUS_LOST:
        case GUI_WIN_EVT_WINDOW_RESIZED:
            state->dirty = 1;
            break;
        default:
            break;
    }
}

int main(int argc, char** argv) {
    gui_create_window_params_t params;
    int window_id;
    int running = 1;

    app_state.render_w = USER_NARCPAD_SURFACE_W;
    app_state.render_h = USER_NARCPAD_SURFACE_H;
    app_state.view_scroll = -1;
    app_copy_text(app_state.title, sizeof(app_state.title), "untitled.txt");
    if (argc > 1 && argv && argv[1]) {
        app_copy_text(app_state.request_path, sizeof(app_state.request_path), argv[1]);
        narcpad_open_path(&app_state);
    } else {
        narcpad_open_new(&app_state);
    }

    params.size = sizeof(params);
    params.flags = 0;
    params.x = -1;
    params.y = -1;
    params.width = USER_NARCPAD_SURFACE_W + 2;
    params.height = USER_NARCPAD_SURFACE_H + 48;
    window_id = user_gui_create_window(&params);
    if (window_id < 0) return 1;
    (void)user_gui_set_title(window_id, app_state.title);
    if (app_resize_surface(window_id, &app_state) != 0) {
        (void)user_gui_destroy_window(window_id);
        return 1;
    }
    app_state.dirty = 1;

    while (running) {
        gui_window_event_t event;
        int blink_phase;
        int needs_render;

        while (user_gui_poll_event(window_id, &event) > 0) {
            if (event.type == GUI_WIN_EVT_CLOSE_REQUEST) {
                running = 0;
                break;
            }
            if (event.type == GUI_WIN_EVT_WINDOW_RESIZED) (void)app_resize_surface(window_id, &app_state);
            app_handle_gui_event(&app_state, &event);
        }

        blink_phase = (int)((user_uptime_ticks() / 20U) & 1U);
        needs_render = app_state.dirty != 0 || app_state.last_blink_phase != blink_phase;
        while (narcpad_dequeue_event(&app_state, &blink_phase, &needs_render)) {
            switch (blink_phase) {
                case USER_NARCPAD_EVT_CHAR: narcpad_append_char(&app_state, (char)needs_render); break;
                case USER_NARCPAD_EVT_BACKSPACE: narcpad_backspace(&app_state); break;
                case USER_NARCPAD_EVT_NEWLINE: narcpad_append_newline(&app_state); break;
                case USER_NARCPAD_EVT_SAVE: narcpad_save(&app_state); break;
                case USER_NARCPAD_EVT_OPEN_NEW: narcpad_open_new(&app_state); break;
                case USER_NARCPAD_EVT_OPEN_PATH: narcpad_open_path(&app_state); break;
                case USER_NARCPAD_EVT_SCROLL: narcpad_scroll_by(&app_state, needs_render); break;
                default: break;
            }
            needs_render = 1;
        }

        if (needs_render) {
            narcpad_render(&app_state);
            app_state.dirty = 0;
            (void)user_gui_set_title(window_id, app_state.title);
            (void)app_present(window_id, &app_state);
        }
        user_sleep(1);
    }

    (void)user_gui_destroy_window(window_id);
    if (app_state.surface) user_free(app_state.surface);
    app_surface_capacity_pixels = 0;
    return 0;
}
