#include <stdint.h>
#include "string.h"
#include "fs.h"
#include "usermode.h"
#include "user_abi.h"
#include "user_gui_lib.h"

#define USER_CODE __attribute__((section(".user_code")))

#include "user_string.h"

#define strlen user_strlen
#define strncpy user_strncpy
#define memset user_memset

static USER_CODE int explorer_append_text(char* dst, int dst_len, const char* src);
static USER_CODE int explorer_append_uint(char* dst, int dst_len, uint32_t value);
static USER_CODE void explorer_build_path_for_idx(int idx, char* out, int out_len);
static USER_CODE int explorer_child_idx_for_row(int parent_idx, int row);

static USER_CODE int explorer_count_children(int parent_idx) {
    disk_fs_node_t node;
    int count = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (user_fs_get_node_info(i, &node) == 0 && node.flags != 0 && node.parent_index == parent_idx) count++;
    }
    return count;
}

static USER_CODE int explorer_compact_layout(int width) {
    return width < 620;
}

static USER_CODE int explorer_sidebar_width(int client_w) {
    if (client_w < 470) return 104;
    if (client_w < 620) return 116;
    return 132;
}

static USER_CODE int explorer_visible_rows(int panel_h) {
    int rows = (panel_h - 64) / 54;
    if (rows < 1) rows = 1;
    return rows;
}

static USER_CODE int explorer_row_from_local_y(user_explorer_state_t* state, int local_y) {
    int panel_y;
    int panel_h;
    int list_y;
    int row_h = 54;
    int visible_rows;
    int row;

    if (!state) return -1;
    panel_y = 36;
    panel_h = state->render_h - 36;
    list_y = panel_y + 42;
    visible_rows = explorer_visible_rows(panel_h);
    if (local_y < list_y || local_y >= list_y + visible_rows * row_h) return -1;
    row = state->list_scroll + ((local_y - list_y) / row_h);
    return row;
}

static USER_CODE void explorer_copy_truncated(char* dst, int dst_len, const char* src, int max_chars) {
    int i = 0;

    if (!dst || dst_len <= 0) return;
    if (!src) src = "";
    if (max_chars > dst_len - 1) max_chars = dst_len - 1;
    while (src[i] != '\0' && i < max_chars) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static USER_CODE void explorer_draw_panel_flat(user_gui_surface_t* surface, int x, int y, int w, int h,
                                               int radius, uint32_t fill, int fill_alpha,
                                               uint32_t border, int border_alpha) {
    user_gui_draw_rounded_rect(surface, x, y, w, h, radius, fill, fill_alpha);
    if (w > 6 && h > 6) {
        user_gui_draw_rounded_rect(surface, x + 1, y + 1, w - 2, h - 2,
                                   radius > 1 ? radius - 1 : radius, 0xFFFFFF, 5);
    }
    user_gui_draw_rounded_rect(surface, x, y, w, h, radius, border, border_alpha);
}

static USER_CODE void explorer_draw_folder_icon(user_gui_surface_t* surface, int x, int y, int selected) {
    user_gui_draw_icon(surface, USER_GUI_ICON_FOLDER, x, y, 28, selected ? UI_ACCENT_ALT : UI_FOLDER, selected);
}

static USER_CODE void explorer_draw_file_icon(user_gui_surface_t* surface, int x, int y, int selected) {
    user_gui_draw_icon(surface, USER_GUI_ICON_FILE, x, y, 28, selected ? UI_ACCENT_ALT : UI_FILE, selected);
}

static USER_CODE void explorer_draw_chip_left(user_gui_surface_t* surface, int x, int y, int w, int h,
                                              uint32_t fill, uint32_t text, const char* label) {
    if (!surface || !label) return;
    user_gui_draw_rounded_rect(surface, x, y, w, h, UI_RADIUS_SM, fill, 235);
    user_gui_draw_rounded_rect(surface, x, y, w, h, UI_RADIUS_SM, UI_BORDER_SOFT, 180);
    user_gui_draw_string(surface, x + 10, y + 6, label, text);
}

static USER_CODE void explorer_draw_list_card(user_gui_surface_t* surface, int x, int y, int w, int h,
                                              int type, const char* name, int size, int selected) {
    uint32_t fill = selected ? UI_ACCENT_DEEP : UI_SURFACE_1;
    uint32_t border = selected ? UI_ACCENT_ALT : UI_BORDER_SOFT;
    char size_buf[16];

    explorer_draw_panel_flat(surface, x, y, w, h, UI_RADIUS_MD, fill, 235, border, 255);
    if (selected) user_gui_fill_rect_alpha(surface, x + 8, y + 6, w - 16, 2, UI_ACCENT_ALT, 180);
    if (type == FS_NODE_DIR) explorer_draw_folder_icon(surface, x + 10, y + 8, selected);
    else explorer_draw_file_icon(surface, x + 10, y + 8, selected);
    user_gui_draw_string(surface, x + 50, y + 11, name, UI_TEXT);
    if (type == FS_NODE_DIR) {
        user_gui_draw_string(surface, x + 50, y + 27, "Directory", selected ? UI_TEXT : UI_TEXT_MUTED);
    } else {
        memset(size_buf, 0, sizeof(size_buf));
        (void)explorer_append_uint(size_buf, sizeof(size_buf), (uint32_t)size);
        user_gui_draw_string(surface, x + 50, y + 27, "File", selected ? UI_TEXT : UI_TEXT_MUTED);
        user_gui_draw_string(surface, x + w - 52, y + 27, size_buf, selected ? UI_TEXT : UI_ACCENT_ALT);
        user_gui_draw_string(surface, x + w - 20, y + 27, "B", selected ? UI_TEXT_MUTED : UI_TEXT_SUBTLE);
    }
}

static USER_CODE void explorer_draw_breadcrumb(user_gui_surface_t* surface, int x, int y, int w, int current_dir) {
    char path[192];
    char shown[28];

    explorer_build_path_for_idx(current_dir, path, sizeof(path));
    explorer_copy_truncated(shown, sizeof(shown), path[0] != '\0' ? path : "/", (w - 40) / 8);
    user_gui_draw_string_tall_shadow(surface, x + 12, y + 9, "Path", UI_TEXT_SUBTLE, UI_SHADOW);
    user_gui_draw_string(surface, x + 58, y + 14, shown, UI_TEXT);
}

static USER_CODE void explorer_draw_modal(user_gui_surface_t* surface, user_explorer_state_t* state) {
    int w = 320;
    int h = 140;
    int x;
    int y;
    disk_fs_node_t node;
    char name_buf[32];

    if (!surface || !state || state->modal_mode == USER_EXPLORER_MODAL_NONE) return;
    x = (surface->width - w) / 2;
    y = (surface->height - h) / 2;
    if (x < 8) x = 8;
    if (y < 8) y = 8;
    if (w > surface->width - 16) w = surface->width - 16;
    if (h > surface->height - 16) h = surface->height - 16;

    user_gui_fill_rect_alpha(surface, 0, 0, surface->width, surface->height, 0x02060A, 110);
    explorer_draw_panel_flat(surface, x, y, w, h, UI_RADIUS_MD, UI_SURFACE_1, 250, UI_BORDER_SOFT, 255);
    if (state->modal_mode == USER_EXPLORER_MODAL_RENAME) {
        user_gui_draw_string_tall_shadow(surface, x + 20, y + 16, "Rename Item", UI_TEXT, UI_SHADOW);
        user_gui_draw_string(surface, x + 20, y + 38, "Type a new name and press Enter.", UI_TEXT_MUTED);
        user_gui_fill_rect(surface, x + 20, y + 60, w - 40, 28, UI_SURFACE_0);
        user_gui_draw_rect(surface, x + 20, y + 60, w - 40, 28, UI_BORDER_SOFT);
        user_gui_draw_string(surface, x + 28, y + 69, state->modal_input, UI_TEXT);
        explorer_draw_chip_left(surface, x + w - 136, y + h - 32, 50, 18, UI_SURFACE_2, UI_TEXT_MUTED, "Esc");
        explorer_draw_chip_left(surface, x + w - 78, y + h - 32, 54, 18, UI_ACCENT_DEEP, UI_TEXT, "Enter");
    } else if (state->modal_mode == USER_EXPLORER_MODAL_DELETE) {
        user_gui_draw_string_tall_shadow(surface, x + 20, y + 16, "Delete Item", UI_DANGER, UI_SHADOW);
        user_gui_draw_string(surface, x + 20, y + 42, "Delete the selected item?", UI_TEXT);
        if (state->selected_idx >= 0 && user_fs_get_node_info(state->selected_idx, &node) == 0 && node.flags != 0) {
            explorer_copy_truncated(name_buf, sizeof(name_buf), node.name, 30);
            user_gui_draw_string(surface, x + 20, y + 60, name_buf, UI_TEXT_MUTED);
        }
        explorer_draw_chip_left(surface, x + w - 136, y + h - 32, 50, 18, UI_SURFACE_2, UI_TEXT_MUTED, "Esc");
        explorer_draw_chip_left(surface, x + w - 78, y + h - 32, 54, 18, UI_DANGER, UI_TEXT, "Delete");
    }
}

static USER_CODE void explorer_render(user_explorer_state_t* state) {
    user_gui_surface_t surface;
    int compact;
    int sidebar_w;
    int content_x;
    int content_w;
    int panel_y;
    int panel_h;
    int list_y;
    int item_count;
    int visible_rows;
    int max_scroll;
    int start_row;
    int row = 0;
    int matched_row = 0;
    char status_buf[24];
    char selected_buf[28];
    disk_fs_node_t node;

    if (!state || !state->surface) return;
    surface.pixels = state->surface;
    surface.width = state->render_w > 0 ? state->render_w : USER_EXPLORER_SURFACE_W;
    surface.height = state->render_h > 0 ? state->render_h : USER_EXPLORER_SURFACE_H;

    compact = explorer_compact_layout(surface.width);
    sidebar_w = explorer_sidebar_width(surface.width);
    content_x = sidebar_w + 12;
    content_w = surface.width - sidebar_w - 12;
    panel_y = 36;
    panel_h = surface.height - 36;
    list_y = panel_y + 42;
    item_count = explorer_count_children(state->current_dir);
    visible_rows = explorer_visible_rows(panel_h);
    max_scroll = item_count > visible_rows ? item_count - visible_rows : 0;
    if (state->list_scroll < 0) state->list_scroll = 0;
    if (state->list_scroll > max_scroll) state->list_scroll = max_scroll;
    start_row = state->list_scroll;

    user_gui_fill_rect(&surface, 0, 0, surface.width, surface.height, UI_SURFACE_1);
    explorer_draw_breadcrumb(&surface, 0, 0, surface.width, state->current_dir);

    explorer_draw_panel_flat(&surface, 0, panel_y, sidebar_w, panel_h, UI_RADIUS_MD, UI_SURFACE_1, 235, UI_BORDER_SOFT, 255);
    user_gui_draw_string_tall_shadow(&surface, 16, panel_y + 10, "Places", UI_TEXT, UI_SHADOW);
    explorer_draw_chip_left(&surface, 12, panel_y + 40, sidebar_w - 24, 22,
                            state->current_dir == -1 ? UI_ACCENT_DEEP : UI_SURFACE_2, UI_TEXT, "Root");
    explorer_draw_chip_left(&surface, 12, panel_y + 68, sidebar_w - 24, 22, UI_SURFACE_2, UI_TEXT, "Desktop");
    explorer_draw_chip_left(&surface, 12, panel_y + 96, sidebar_w - 24, 22, UI_SURFACE_2, UI_TEXT,
                            compact ? "Home" : "Workspace");
    user_gui_fill_rect_alpha(&surface, 12, panel_y + 132, sidebar_w - 24, 1, UI_BORDER_SOFT, 255);
    user_gui_draw_string(&surface, 16, panel_y + 146, "Items", UI_TEXT_SUBTLE);
    user_gui_draw_int(&surface, 64, panel_y + 146, item_count, UI_ACCENT_ALT);
    user_gui_draw_string(&surface, 16, panel_y + panel_h - 44, "Status", UI_TEXT_SUBTLE);
    if (state->selected_idx >= 0 && user_fs_get_node_info(state->selected_idx, &node) == 0 && node.flags != 0) {
        explorer_copy_truncated(selected_buf, sizeof(selected_buf), node.name, compact ? 11 : 15);
        user_gui_draw_string(&surface, 16, panel_y + panel_h - 28, selected_buf, UI_TEXT);
    } else {
        user_gui_draw_string(&surface, 16, panel_y + panel_h - 28, "No selection", UI_TEXT_MUTED);
    }

    explorer_draw_panel_flat(&surface, content_x, panel_y, content_w, panel_h, UI_RADIUS_MD, UI_SURFACE_1, 235, UI_BORDER_SOFT, 255);
    user_gui_draw_string_tall_shadow(&surface, content_x + 14, panel_y + 10, "Directory", UI_TEXT, UI_SHADOW);
    user_gui_draw_string(&surface, content_x + content_w - (compact ? 38 : 72), panel_y + 16,
                         compact ? "Sync" : "Refresh", UI_TEXT_SUBTLE);
    user_gui_fill_rect_alpha(&surface, content_x + 12, panel_y + 30, content_w - 24, 1, UI_BORDER_SOFT, 255);

    if (item_count == 0) {
        explorer_draw_panel_flat(&surface, content_x + 16, list_y + 28, content_w - 32, 96,
                                 UI_RADIUS_MD, UI_SURFACE_0, 255, UI_BORDER_SOFT, 255);
        explorer_draw_folder_icon(&surface, content_x + 34, list_y + 52, 0);
        user_gui_draw_string(&surface, content_x + 82, list_y + 58, "This directory is empty.", UI_TEXT);
        user_gui_draw_string(&surface, content_x + 82, list_y + 76,
                             compact ? "Create a file or folder to begin." :
                                       "Create a file or folder to start using this workspace.",
                             UI_TEXT_MUTED);
    } else {
        for (int i = 0; i < MAX_FILES; i++) {
            if (user_fs_get_node_info(i, &node) != 0 || node.flags == 0 || node.parent_index != state->current_dir) continue;
            if (matched_row >= start_row && row < visible_rows) {
                explorer_draw_list_card(&surface, content_x + 16, list_y + row * 54, content_w - 32, 44,
                                        node.flags, node.name, (int)node.size, state->selected_idx == i);
                row++;
            }
            matched_row++;
            if (row >= visible_rows) break;
        }
    }

    user_gui_fill_rect_alpha(&surface, content_x + 12, panel_y + panel_h - 26, content_w - 24, 14, UI_SURFACE_0, 255);
    memset(status_buf, 0, sizeof(status_buf));
    (void)explorer_append_uint(status_buf, sizeof(status_buf), (uint32_t)item_count);
    user_gui_draw_string(&surface, content_x + 18, panel_y + panel_h - 22, status_buf, UI_ACCENT_ALT);
    user_gui_draw_string(&surface, content_x + 26 + (int)strlen(status_buf) * 8, panel_y + panel_h - 22,
                         "items", UI_TEXT_SUBTLE);

    explorer_draw_modal(&surface, state);
}

static USER_CODE int explorer_dequeue_event(user_explorer_state_t* state, int* out_type, int* out_value) {
    int head;

    if (!state || !out_type || !out_value) return 0;
    if (state->event_head == state->event_tail) return 0;
    head = state->event_head;
    *out_type = state->event_type[head];
    *out_value = state->event_arg[head];
    state->event_head = (head + 1) % USER_GUI_EVENT_QUEUE_CAP;
    return 1;
}

static USER_CODE int explorer_append_text(char* dst, int dst_len, const char* src) {
    int off = 0;

    if (!dst || dst_len <= 0 || !src) return -1;
    while (dst[off] != '\0') off++;
    while (*src) {
        if (off + 1 >= dst_len) return -1;
        dst[off++] = *src++;
    }
    dst[off] = '\0';
    return 0;
}

static USER_CODE int explorer_append_uint(char* dst, int dst_len, uint32_t value) {
    char digits[12];
    int count = 0;

    if (value == 0U) return explorer_append_text(dst, dst_len, "0");
    while (value != 0U && count < (int)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (count > 0) {
        char c[2];
        c[0] = digits[--count];
        c[1] = '\0';
        if (explorer_append_text(dst, dst_len, c) != 0) return -1;
    }
    return 0;
}

static USER_CODE int explorer_selected_valid(user_explorer_state_t* state, disk_fs_node_t* out_node) {
    if (!state || state->selected_idx < 0) return 0;
    return user_fs_get_node_info(state->selected_idx, out_node) == 0;
}

static USER_CODE int explorer_child_idx_for_row(int parent_idx, int row) {
    disk_fs_node_t node;
    int current_row = 0;

    if (row < 0) return -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (user_fs_get_node_info(i, &node) != 0 || node.flags == 0 || node.parent_index != parent_idx) continue;
        if (current_row == row) return i;
        current_row++;
    }
    return -1;
}

static USER_CODE void explorer_cancel_modal(user_explorer_state_t* state) {
    if (!state) return;
    state->modal_mode = USER_EXPLORER_MODAL_NONE;
    state->modal_input_len = 0;
    state->modal_input[0] = '\0';
    state->dirty = 1;
}

static USER_CODE void explorer_open_dir(user_explorer_state_t* state, int dir_idx) {
    disk_fs_node_t node;

    if (!state || dir_idx < -1) return;
    if (dir_idx >= 0) {
        if (user_fs_get_node_info(dir_idx, &node) != 0) return;
        if (node.flags != FS_NODE_DIR) return;
    }
    if (state->current_dir != dir_idx) state->prev_dir = state->current_dir;
    state->current_dir = dir_idx;
    state->selected_idx = -1;
    state->list_scroll = 0;
    explorer_cancel_modal(state);
    state->dirty = 1;
}

static USER_CODE void explorer_build_path_for_idx(int idx, char* out, int out_len) {
    if (!out || out_len <= 0) return;
    out[0] = '\0';
    (void)user_fs_get_path(idx, out, (uint32_t)out_len);
}

static USER_CODE void explorer_open_selected(user_explorer_state_t* state) {
    disk_fs_node_t node;
    char path[256];
    const char* argv[2];

    if (!explorer_selected_valid(state, &node)) return;
    if (node.flags == FS_NODE_DIR) {
        explorer_open_dir(state, state->selected_idx);
        return;
    }
    explorer_build_path_for_idx(state->selected_idx, path, sizeof(path));
    if (path[0] == '\0') return;
    argv[0] = "/bin/narcpad";
    argv[1] = path;
    (void)user_spawn("/bin/narcpad", argv, 2U);
}

static USER_CODE void explorer_create_in_current_dir(user_explorer_state_t* state, int is_dir) {
    char path[320];
    int n;

    if (!state) return;
    for (n = 1; n < 100; n++) {
        path[0] = '\0';
        explorer_build_path_for_idx(state->current_dir, path, sizeof(path));
        if (!(path[0] == '/' && path[1] == '\0')) {
            if (explorer_append_text(path, sizeof(path), "/") != 0) return;
        }
        if (is_dir) {
            if (explorer_append_text(path, sizeof(path), "NewFolder") != 0) return;
        } else {
            if (explorer_append_text(path, sizeof(path), "NewFile") != 0) return;
        }
        if (n > 1) {
            if (explorer_append_uint(path, sizeof(path), (uint32_t)n) != 0) return;
        }
        if (!is_dir && explorer_append_text(path, sizeof(path), ".txt") != 0) return;
        if (user_fs_find_node(path) == -1) {
            if (is_dir) {
                if (user_fs_mkdir(path) == 0) state->dirty = 1;
            } else {
                if (user_fs_touch(path) == 0) state->dirty = 1;
            }
            return;
        }
    }
}

static USER_CODE void explorer_begin_rename(user_explorer_state_t* state) {
    disk_fs_node_t node;
    int i = 0;

    if (!explorer_selected_valid(state, &node)) return;
    while (node.name[i] != '\0' && i < (int)sizeof(state->modal_input) - 1) {
        state->modal_input[i] = node.name[i];
        i++;
    }
    state->modal_input[i] = '\0';
    state->modal_input_len = i;
    state->modal_mode = USER_EXPLORER_MODAL_RENAME;
    state->dirty = 1;
}

static USER_CODE void explorer_begin_delete(user_explorer_state_t* state) {
    disk_fs_node_t node;

    if (!explorer_selected_valid(state, &node)) return;
    state->modal_mode = USER_EXPLORER_MODAL_DELETE;
    state->dirty = 1;
}

static USER_CODE void explorer_modal_append_char(user_explorer_state_t* state, char c) {
    if (!state || state->modal_mode != USER_EXPLORER_MODAL_RENAME) return;
    if (c == '\0' || c == '/' || state->modal_input_len >= (int)sizeof(state->modal_input) - 1) return;
    state->modal_input[state->modal_input_len++] = c;
    state->modal_input[state->modal_input_len] = '\0';
    state->dirty = 1;
}

static USER_CODE void explorer_modal_backspace(user_explorer_state_t* state) {
    if (!state || state->modal_mode != USER_EXPLORER_MODAL_RENAME || state->modal_input_len <= 0) return;
    state->modal_input_len--;
    state->modal_input[state->modal_input_len] = '\0';
    state->dirty = 1;
}

static USER_CODE void explorer_delete_selected(user_explorer_state_t* state) {
    char path[256];
    disk_fs_node_t node;

    if (!explorer_selected_valid(state, &node)) return;
    explorer_build_path_for_idx(state->selected_idx, path, sizeof(path));
    if (path[0] == '\0') return;
    if (user_fs_delete(path) == 0) {
        state->selected_idx = -1;
        state->dirty = 1;
    }
}

static USER_CODE void explorer_submit_modal(user_explorer_state_t* state) {
    char path[256];
    disk_fs_node_t node;

    if (!state) return;
    if (state->modal_mode == USER_EXPLORER_MODAL_RENAME) {
        if (state->modal_input[0] != '\0' && explorer_selected_valid(state, &node)) {
            explorer_build_path_for_idx(state->selected_idx, path, sizeof(path));
            if (path[0] != '\0' && user_fs_rename(path, state->modal_input) == 0) {
                state->dirty = 1;
            }
        }
        explorer_cancel_modal(state);
    } else if (state->modal_mode == USER_EXPLORER_MODAL_DELETE) {
        explorer_delete_selected(state);
        explorer_cancel_modal(state);
    }
}

static USER_CODE void explorer_move_selected_to(user_explorer_state_t* state, int target_dir) {
    char source_path[256];
    char target_path[256];
    disk_fs_node_t source_node;
    disk_fs_node_t target_node;

    if (!explorer_selected_valid(state, &source_node)) return;
    if (target_dir >= 0) {
        if (user_fs_get_node_info(target_dir, &target_node) != 0 || target_node.flags != FS_NODE_DIR) return;
    }
    explorer_build_path_for_idx(state->selected_idx, source_path, sizeof(source_path));
    explorer_build_path_for_idx(target_dir, target_path, sizeof(target_path));
    if (source_path[0] == '\0' || target_path[0] == '\0') return;
    if (user_fs_move(source_path, target_path) == 0) {
        state->selected_idx = -1;
        state->dirty = 1;
    }
}

static USER_CODE void explorer_go_up(user_explorer_state_t* state) {
    disk_fs_node_t node;

    if (!state || state->current_dir < 0) return;
    if (user_fs_get_node_info(state->current_dir, &node) != 0) return;
    explorer_open_dir(state, node.parent_index);
}

static USER_CODE void explorer_scroll_by(user_explorer_state_t* state, int wheel_steps) {
    int item_count;
    int panel_h;
    int visible_rows;
    int max_scroll;

    if (!state || wheel_steps == 0) return;
    item_count = explorer_count_children(state->current_dir);
    panel_h = state->render_h - 36;
    visible_rows = explorer_visible_rows(panel_h);
    max_scroll = item_count > visible_rows ? item_count - visible_rows : 0;
    state->list_scroll -= wheel_steps * 3;
    if (state->list_scroll < 0) state->list_scroll = 0;
    if (state->list_scroll > max_scroll) state->list_scroll = max_scroll;
    state->dirty = 1;
}

static USER_CODE void explorer_handle_pointer_down(user_explorer_state_t* state, int local_x, int local_y) {
    int compact;
    int sidebar_w;
    int content_x;
    int content_w;
    int panel_y;
    int row;
    int child_idx;

    if (!state) return;
    if (state->modal_mode != USER_EXPLORER_MODAL_NONE) {
        int modal_w = 320;
        int modal_h = 140;
        int modal_x = (state->render_w - modal_w) / 2;
        int modal_y = (state->render_h - modal_h) / 2;

        if (local_x >= modal_x + modal_w - 136 && local_x <= modal_x + modal_w - 86 &&
            local_y >= modal_y + modal_h - 32 && local_y <= modal_y + modal_h - 14) {
            explorer_cancel_modal(state);
            return;
        }
        if (local_x >= modal_x + modal_w - 78 && local_x <= modal_x + modal_w - 24 &&
            local_y >= modal_y + modal_h - 32 && local_y <= modal_y + modal_h - 14) {
            explorer_submit_modal(state);
        }
        return;
    }

    compact = explorer_compact_layout(state->render_w);
    sidebar_w = explorer_sidebar_width(state->render_w);
    content_x = sidebar_w + 12;
    content_w = state->render_w - sidebar_w - 12;
    panel_y = 36;

    state->drag_candidate_idx = -1;
    if (local_y >= 0 && local_y <= 28 && local_x >= 0 && local_x <= state->render_w) {
        explorer_open_dir(state, -1);
        return;
    }
    if (local_y >= panel_y + 8 && local_y <= panel_y + 24 &&
        local_x >= content_x + content_w - (compact ? 54 : 72) && local_x <= content_x + content_w - 8) {
        state->dirty = 1;
        return;
    }
    if (local_x >= 12 && local_x <= sidebar_w - 12) {
        if (local_y >= panel_y + 40 && local_y <= panel_y + 62) {
            explorer_open_dir(state, -1);
            return;
        }
        if (local_y >= panel_y + 68 && local_y <= panel_y + 90) {
            int desktop_idx = user_fs_find_node("/desktop");
            explorer_open_dir(state, desktop_idx >= 0 ? desktop_idx : -1);
            return;
        }
        if (local_y >= panel_y + 96 && local_y <= panel_y + 118) {
            int home_idx = user_fs_find_node("/home/user");
            if (home_idx >= 0) explorer_open_dir(state, home_idx);
            return;
        }
    }

    if (local_x < content_x + 16 || local_x > content_x + content_w - 16) {
        state->selected_idx = -1;
        state->dirty = 1;
        return;
    }

    row = explorer_row_from_local_y(state, local_y);
    if (row < 0) {
        state->selected_idx = -1;
        state->dirty = 1;
        return;
    }
    child_idx = explorer_child_idx_for_row(state->current_dir, row);
    state->selected_idx = child_idx;
    state->drag_candidate_idx = child_idx;
    state->dirty = 1;
}

static USER_CODE void explorer_handle_pointer_up(user_explorer_state_t* state, int local_x, int local_y) {
    int sidebar_w;
    int content_x;
    int content_w;
    int panel_y;
    int row;
    int target_idx;
    disk_fs_node_t node;

    if (!state || state->drag_candidate_idx < 0 || state->selected_idx != state->drag_candidate_idx) return;

    sidebar_w = explorer_sidebar_width(state->render_w);
    content_x = sidebar_w + 12;
    content_w = state->render_w - sidebar_w - 12;
    panel_y = 36;

    if (local_x >= 12 && local_x <= sidebar_w - 12) {
        if (local_y >= panel_y + 40 && local_y <= panel_y + 62) {
            explorer_move_selected_to(state, -1);
        } else if (local_y >= panel_y + 68 && local_y <= panel_y + 90) {
            int desktop_idx = user_fs_find_node("/desktop");
            explorer_move_selected_to(state, desktop_idx >= 0 ? desktop_idx : -1);
        } else if (local_y >= panel_y + 96 && local_y <= panel_y + 118) {
            int home_idx = user_fs_find_node("/home/user");
            if (home_idx >= 0) explorer_move_selected_to(state, home_idx);
        }
    } else if (local_x >= content_x + 16 && local_x <= content_x + content_w - 16) {
        row = explorer_row_from_local_y(state, local_y);
        target_idx = explorer_child_idx_for_row(state->current_dir, row);
        if (target_idx >= 0 && target_idx != state->selected_idx &&
            user_fs_get_node_info(target_idx, &node) == 0 && node.flags == FS_NODE_DIR) {
            explorer_move_selected_to(state, target_idx);
        }
    }
    state->drag_candidate_idx = -1;
}

static USER_CODE void explorer_handle_pointer_dblclick(user_explorer_state_t* state, int local_x, int local_y) {
    int content_x;
    int content_w;
    int row;
    int child_idx;
    int sidebar_w;

    if (!state || state->modal_mode != USER_EXPLORER_MODAL_NONE) return;
    sidebar_w = explorer_sidebar_width(state->render_w);
    content_x = sidebar_w + 12;
    content_w = state->render_w - sidebar_w - 12;
    if (local_x < content_x + 16 || local_x > content_x + content_w - 16) return;
    row = explorer_row_from_local_y(state, local_y);
    child_idx = explorer_child_idx_for_row(state->current_dir, row);
    if (child_idx < 0) return;
    state->selected_idx = child_idx;
    explorer_open_selected(state);
    state->drag_candidate_idx = -1;
    state->dirty = 1;
}

void USER_CODE user_explorer_entry_c(user_explorer_state_t* state) {
    if (!state) return;

    for (;;) {
        int event_type;
        int event_value;
        int needs_render = state->dirty != 0;

        while (explorer_dequeue_event(state, &event_type, &event_value)) {
            switch (event_type) {
                case USER_EXPLORER_EVT_OPEN_DIR:
                    explorer_open_dir(state, event_value);
                    break;
                case USER_EXPLORER_EVT_SELECT_IDX:
                    state->selected_idx = event_value;
                    state->dirty = 1;
                    break;
                case USER_EXPLORER_EVT_OPEN_SELECTED:
                    explorer_open_selected(state);
                    break;
                case USER_EXPLORER_EVT_CREATE_FILE:
                    explorer_create_in_current_dir(state, 0);
                    break;
                case USER_EXPLORER_EVT_CREATE_DIR:
                    explorer_create_in_current_dir(state, 1);
                    break;
                case USER_EXPLORER_EVT_BEGIN_RENAME:
                    explorer_begin_rename(state);
                    break;
                case USER_EXPLORER_EVT_BEGIN_DELETE:
                    explorer_begin_delete(state);
                    break;
                case USER_EXPLORER_EVT_MODAL_CHAR:
                    explorer_modal_append_char(state, (char)event_value);
                    break;
                case USER_EXPLORER_EVT_MODAL_BACKSPACE:
                    explorer_modal_backspace(state);
                    break;
                case USER_EXPLORER_EVT_MODAL_SUBMIT:
                    explorer_submit_modal(state);
                    break;
                case USER_EXPLORER_EVT_MODAL_CANCEL:
                    explorer_cancel_modal(state);
                    break;
                case USER_EXPLORER_EVT_MOVE_SELECTED_TO:
                    explorer_move_selected_to(state, event_value);
                    break;
                case USER_EXPLORER_EVT_GO_BACK:
                    explorer_open_dir(state, state->prev_dir);
                    break;
                case USER_EXPLORER_EVT_GO_UP:
                    explorer_go_up(state);
                    break;
                case USER_EXPLORER_EVT_REFRESH:
                    state->dirty = 1;
                    break;
                case USER_EXPLORER_EVT_POINTER_DOWN:
                    explorer_handle_pointer_down(state,
                                                 USER_EXPLORER_POINT_X(event_value),
                                                 USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_UP:
                    explorer_handle_pointer_up(state,
                                               USER_EXPLORER_POINT_X(event_value),
                                               USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_DBLCLICK:
                    explorer_handle_pointer_dblclick(state,
                                                     USER_EXPLORER_POINT_X(event_value),
                                                     USER_EXPLORER_POINT_Y(event_value));
                    break;
                case USER_EXPLORER_EVT_POINTER_WHEEL:
                    explorer_scroll_by(state, event_value);
                    break;
                default:
                    break;
            }
            needs_render = 1;
        }
        if (needs_render) {
            explorer_render(state);
            state->dirty = 0;
        }
        user_yield();
    }
}
