#include "user_lib.h"

#define MAPLE_MONO_8X8_SYMBOL user_gui_font
#include "maple_mono_8x8.h"
#undef MAPLE_MONO_8X8_SYMBOL

#include "../../kernel/apps/user_explorer.c"

static user_explorer_state_t app_state;
static size_t app_surface_capacity_pixels;

static int app_resize_surface(int window_id, user_explorer_state_t* state) {
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

static void app_queue_event(user_explorer_state_t* state, int type, int value) {
    int next_tail;

    if (!state) return;
    next_tail = (state->event_tail + 1) % USER_GUI_EVENT_QUEUE_CAP;
    if (next_tail == state->event_head) return;
    state->event_type[state->event_tail] = (uint16_t)type;
    state->event_arg[state->event_tail] = value;
    state->event_tail = next_tail;
}

static int app_pack_point(int x, int y) {
    return USER_EXPLORER_PACK_POINT(x, y);
}

static int app_present(int window_id, user_explorer_state_t* state) {
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

static int app_dir_from_path(const char* path) {
    int idx;
    disk_fs_node_t node;

    if (!path || path[0] == '\0') return -1;
    idx = user_fs_find_node(path);
    if (idx < 0) return -1;
    if (user_fs_get_node_info(idx, &node) != 0 || node.flags != FS_NODE_DIR) return -1;
    return idx;
}

int main(int argc, char** argv) {
    gui_create_window_params_t params;
    int window_id;
    int initial_dir = -1;
    int running = 1;

    if (argc > 1 && argv && argv[1]) initial_dir = app_dir_from_path(argv[1]);
    app_state.render_w = USER_EXPLORER_SURFACE_W;
    app_state.render_h = USER_EXPLORER_SURFACE_H;
    app_state.current_dir = initial_dir;
    app_state.prev_dir = -1;
    app_state.selected_idx = -1;
    app_state.drag_candidate_idx = -1;
    app_state.dirty = 1;

    params.size = sizeof(params);
    params.flags = 0;
    params.x = -1;
    params.y = -1;
    params.width = 980;
    params.height = 650;
    window_id = user_gui_create_window(&params);
    if (window_id < 0) return 1;
    (void)user_gui_set_title(window_id, "Explorer");
    if (app_resize_surface(window_id, &app_state) != 0) {
        (void)user_gui_destroy_window(window_id);
        return 1;
    }

    while (running) {
        gui_window_event_t event;
        int event_type;
        int event_value;
        int needs_render = app_state.dirty != 0;

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
                app_queue_event(&app_state,
                                event.arg2 == 2 ? USER_EXPLORER_EVT_POINTER_DBLCLICK
                                                : USER_EXPLORER_EVT_POINTER_DOWN,
                                app_pack_point(event.arg0, event.arg1));
            } else if (event.type == GUI_WIN_EVT_MOUSE_UP) {
                app_queue_event(&app_state, USER_EXPLORER_EVT_POINTER_UP,
                                app_pack_point(event.arg0, event.arg1));
            } else if (event.type == GUI_WIN_EVT_MOUSE_WHEEL) {
                app_queue_event(&app_state, USER_EXPLORER_EVT_POINTER_WHEEL, event.arg2);
            } else if (event.type == GUI_WIN_EVT_KEY_DOWN) {
                if (event.arg0 == 0x0E) app_queue_event(&app_state, USER_EXPLORER_EVT_MODAL_BACKSPACE, 0);
                else if (event.arg0 == 0x1C) app_queue_event(&app_state, USER_EXPLORER_EVT_MODAL_SUBMIT, 0);
                else if (event.arg0 == 0x01) app_queue_event(&app_state, USER_EXPLORER_EVT_MODAL_CANCEL, 0);
            } else if (event.type == GUI_WIN_EVT_CHAR) {
                if (event.arg0 != 0 && event.arg0 != '\b' && event.arg0 != '\n') {
                    app_queue_event(&app_state, USER_EXPLORER_EVT_MODAL_CHAR, event.arg0);
                }
            } else if (event.type == GUI_WIN_EVT_PAINT ||
                       event.type == GUI_WIN_EVT_FOCUS_GAINED ||
                       event.type == GUI_WIN_EVT_FOCUS_LOST) {
                app_state.dirty = 1;
                needs_render = 1;
            }
        }

        while (explorer_dequeue_event(&app_state, &event_type, &event_value)) {
            switch (event_type) {
                case USER_EXPLORER_EVT_OPEN_DIR: explorer_open_dir(&app_state, event_value); break;
                case USER_EXPLORER_EVT_SELECT_IDX: app_state.selected_idx = event_value; app_state.dirty = 1; break;
                case USER_EXPLORER_EVT_OPEN_SELECTED: explorer_open_selected(&app_state); break;
                case USER_EXPLORER_EVT_CREATE_FILE: explorer_create_in_current_dir(&app_state, 0); break;
                case USER_EXPLORER_EVT_CREATE_DIR: explorer_create_in_current_dir(&app_state, 1); break;
                case USER_EXPLORER_EVT_BEGIN_RENAME: explorer_begin_rename(&app_state); break;
                case USER_EXPLORER_EVT_BEGIN_DELETE: explorer_begin_delete(&app_state); break;
                case USER_EXPLORER_EVT_MODAL_CHAR: explorer_modal_append_char(&app_state, (char)event_value); break;
                case USER_EXPLORER_EVT_MODAL_BACKSPACE: explorer_modal_backspace(&app_state); break;
                case USER_EXPLORER_EVT_MODAL_SUBMIT: explorer_submit_modal(&app_state); break;
                case USER_EXPLORER_EVT_MODAL_CANCEL: explorer_cancel_modal(&app_state); break;
                case USER_EXPLORER_EVT_MOVE_SELECTED_TO: explorer_move_selected_to(&app_state, event_value); break;
                case USER_EXPLORER_EVT_GO_BACK: explorer_open_dir(&app_state, app_state.prev_dir); break;
                case USER_EXPLORER_EVT_GO_UP: explorer_go_up(&app_state); break;
                case USER_EXPLORER_EVT_REFRESH: app_state.dirty = 1; break;
                case USER_EXPLORER_EVT_POINTER_DOWN:
                    explorer_handle_pointer_down(&app_state,
                                                 USER_EXPLORER_POINT_X(event_value),
                                                 USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_UP:
                    explorer_handle_pointer_up(&app_state,
                                               USER_EXPLORER_POINT_X(event_value),
                                               USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_DBLCLICK:
                    explorer_handle_pointer_dblclick(&app_state,
                                                     USER_EXPLORER_POINT_X(event_value),
                                                     USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_WHEEL: explorer_scroll_by(&app_state, event_value); break;
                default: break;
            }
            needs_render = 1;
        }

        if (needs_render) {
            explorer_render(&app_state);
            app_state.dirty = 0;
            (void)app_present(window_id, &app_state);
        }
        user_sleep(1);
    }

    (void)user_gui_destroy_window(window_id);
    if (app_state.surface) user_free(app_state.surface);
    app_surface_capacity_pixels = 0;
    return 0;
}
