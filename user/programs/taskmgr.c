#include "user_lib.h"

#define MAPLE_MONO_8X8_SYMBOL user_gui_font
#include "maple_mono_8x8.h"
#undef MAPLE_MONO_8X8_SYMBOL
#include "user_gui_lib.h"

#define TASKMGR_W 620
#define TASKMGR_H 430
#define TASKMGR_MAX_PROCS 16
#define TASKMGR_ROW_H 24
#define TASKMGR_HEADER_H 92

static uint32_t* surface;
static size_t surface_capacity_pixels;
static int render_w;
static int render_h;
static int selected_pid;
static int scroll_row;
static user_gui_surface_t render_surface;
static process_snapshot_entry_t procs[TASKMGR_MAX_PROCS];
static int proc_count;
static system_info_t sysinfo;

static int str_len(const char* text) {
    int len = 0;
    if (!text) return 0;
    while (text[len]) len++;
    return len;
}

static void copy_text_local(char* dst, int dst_len, const char* src) {
    int i = 0;
    if (!dst || dst_len <= 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < dst_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void append_text(char* dst, int dst_len, const char* src) {
    int pos = str_len(dst);
    int i = 0;
    if (!dst || !src || pos >= dst_len) return;
    while (src[i] && pos + 1 < dst_len) dst[pos++] = src[i++];
    dst[pos] = '\0';
}

static void append_u32(char* dst, int dst_len, uint32_t value) {
    char tmp[16];
    int pos = 0;

    if (value == 0U) {
        append_text(dst, dst_len, "0");
        return;
    }
    while (value && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (pos > 0) {
        char one[2];
        one[0] = tmp[--pos];
        one[1] = '\0';
        append_text(dst, dst_len, one);
    }
}

static void format_mb(char* out, int out_len, uint64_t bytes) {
    uint32_t mb = (uint32_t)(bytes / (1024ULL * 1024ULL));

    copy_text_local(out, out_len, "");
    append_u32(out, out_len, mb);
    append_text(out, out_len, " MB");
}

static const char* state_name(int state) {
    switch (state) {
        case 1: return "runnable";
        case 2: return "running";
        case 3: return "zombie";
        default: return "unknown";
    }
}

static const char* kind_name(int kind) {
    return kind == 1 ? "user" : "kernel";
}

static void draw_text(user_gui_surface_t* s, int x, int y, const char* text, uint32_t color) {
    if (!s || !text) return;
    while (*text) {
        user_gui_draw_char(s, x, y, *text, color);
        x += 8;
        text++;
    }
}

static void draw_text_clip(user_gui_surface_t* s, int x, int y, int max_w, const char* text, uint32_t color) {
    int chars;

    if (!s || !text || max_w <= 0) return;
    chars = max_w / 8;
    while (*text && chars-- > 0) {
        user_gui_draw_char(s, x, y, *text, color);
        x += 8;
        text++;
    }
}

static void draw_label_value(user_gui_surface_t* s, int x, int y, const char* label, const char* value) {
    draw_text(s, x, y, label, UI_TEXT_MUTED);
    draw_text(s, x + str_len(label) * 8 + 8, y, value, UI_TEXT);
}

static int resize_surface(int window_id) {
    gui_window_info_t info;
    size_t pixels;
    uint32_t* next;

    if (user_gui_get_window_info(window_id, &info) != 0) return -1;
    render_w = info.client_width > 1 ? info.client_width : 1;
    render_h = info.client_height > 1 ? info.client_height : 1;
    pixels = (size_t)render_w * (size_t)render_h;
    if (!surface || pixels > surface_capacity_pixels) {
        next = (uint32_t*)user_malloc(pixels * sizeof(uint32_t));
        if (!next) return -1;
        if (surface) user_free(surface);
        surface = next;
        surface_capacity_pixels = pixels;
    }
    return 0;
}

static void refresh_data(void) {
    if (user_system_info(&sysinfo) != 0) {
        userlib_memset(&sysinfo, 0, sizeof(sysinfo));
    }
    proc_count = user_process_snapshot(procs, TASKMGR_MAX_PROCS);
    if (proc_count < 0) proc_count = 0;
    if (selected_pid <= 0 && proc_count > 0) selected_pid = procs[0].pid;
}

static void render_taskmgr(void) {
    char total[24];
    char used[24];
    char free_mem[24];
    char installed[24];
    char value[32];
    int table_y = TASKMGR_HEADER_H;
    int rows_visible;
    int used_pct = 0;

    if (!surface || render_w <= 0 || render_h <= 0) return;
    render_surface.pixels = surface;
    render_surface.width = render_w;
    render_surface.height = render_h;
    user_gui_fill_rect(&render_surface, 0, 0, render_w, render_h, UI_DESKTOP_TOP);
    user_gui_fill_rect(&render_surface, 0, 0, render_w, 54, UI_SURFACE_0);
    user_gui_fill_rect(&render_surface, 0, 54, render_w, 1, UI_BORDER_SOFT);
    draw_text(&render_surface, 18, 16, "Task Manager", UI_TEXT);
    draw_text(&render_surface, render_w - 154, 16, "[R] Refresh", UI_TEXT_MUTED);
    draw_text(&render_surface, render_w - 154, 30, "[Del] Kill", UI_DANGER);

    format_mb(total, sizeof(total), sysinfo.total_memory_bytes);
    format_mb(used, sizeof(used), sysinfo.used_memory_bytes);
    format_mb(free_mem, sizeof(free_mem), sysinfo.free_memory_bytes);
    format_mb(installed, sizeof(installed), sysinfo.installed_memory_bytes);
    if (sysinfo.total_memory_bytes != 0ULL) {
        used_pct = (int)((sysinfo.used_memory_bytes * 100ULL) / sysinfo.total_memory_bytes);
    }

    draw_label_value(&render_surface, 18, 68, "RAM", used);
    draw_text(&render_surface, 138, 68, "/", UI_TEXT_MUTED);
    draw_text(&render_surface, 154, 68, total, UI_TEXT_MUTED);
    user_gui_draw_rounded_rect(&render_surface, 250, 65, 180, 14, 4, UI_SURFACE_1, 255);
    user_gui_fill_rect(&render_surface, 253, 68, (174 * used_pct) / 100, 8, UI_ACCENT);
    draw_label_value(&render_surface, 452, 68, "Free", free_mem);
    draw_label_value(&render_surface, 18, 82, "Installed", installed);
    copy_text_local(value, sizeof(value), "");
    append_u32(value, sizeof(value), sysinfo.process_count);
    draw_label_value(&render_surface, 250, 82, "Processes", value);

    user_gui_fill_rect(&render_surface, 0, table_y, render_w, 24, UI_SURFACE_1);
    draw_text(&render_surface, 18, table_y + 8, "PID", UI_TEXT_MUTED);
    draw_text(&render_surface, 70, table_y + 8, "PPID", UI_TEXT_MUTED);
    draw_text(&render_surface, 128, table_y + 8, "STATE", UI_TEXT_MUTED);
    draw_text(&render_surface, 224, table_y + 8, "KIND", UI_TEXT_MUTED);
    draw_text(&render_surface, 290, table_y + 8, "MEM", UI_TEXT_MUTED);
    draw_text(&render_surface, 370, table_y + 8, "NAME", UI_TEXT_MUTED);

    rows_visible = (render_h - table_y - 30) / TASKMGR_ROW_H;
    if (rows_visible < 1) rows_visible = 1;
    if (scroll_row > proc_count - rows_visible) scroll_row = proc_count - rows_visible;
    if (scroll_row < 0) scroll_row = 0;

    for (int row = 0; row < rows_visible; row++) {
        int idx = scroll_row + row;
        int y = table_y + 24 + row * TASKMGR_ROW_H;
        uint32_t row_bg = (row & 1) ? 0x101820 : 0x0D141B;

        if (idx >= proc_count) break;
        if (procs[idx].pid == selected_pid) row_bg = 0x1C3A52;
        user_gui_fill_rect(&render_surface, 0, y, render_w, TASKMGR_ROW_H, row_bg);
        copy_text_local(value, sizeof(value), "");
        append_u32(value, sizeof(value), (uint32_t)procs[idx].pid);
        draw_text(&render_surface, 18, y + 8, value, UI_TEXT);
        copy_text_local(value, sizeof(value), "");
        append_u32(value, sizeof(value), (uint32_t)procs[idx].parent_pid);
        draw_text(&render_surface, 70, y + 8, value, UI_TEXT_MUTED);
        draw_text(&render_surface, 128, y + 8, state_name(procs[idx].state), UI_TEXT);
        draw_text(&render_surface, 224, y + 8, kind_name(procs[idx].kind), UI_TEXT_MUTED);
        format_mb(value, sizeof(value), procs[idx].memory_bytes);
        draw_text(&render_surface, 290, y + 8, value, UI_SUCCESS);
        draw_text_clip(&render_surface, 370, y + 8, render_w - 388, procs[idx].name, UI_TEXT);
    }
}

static int present_surface(int window_id) {
    gui_present_params_t present;

    if (!surface || render_w <= 0 || render_h <= 0) return -1;
    present.size = sizeof(present);
    present.flags = 0;
    present.buffer_ptr = (uintptr_t)surface;
    present.x = 0;
    present.y = 0;
    present.width = (uint32_t)render_w;
    present.height = (uint32_t)render_h;
    present.stride_bytes = (uint32_t)render_w * 4U;
    return user_gui_present(window_id, &present);
}

int main(void) {
    gui_create_window_params_t params;
    int window_id;
    int running = 1;
    int dirty = 1;
    uint32_t last_refresh = 0;

    params.size = sizeof(params);
    params.flags = 0;
    params.x = -1;
    params.y = -1;
    params.width = TASKMGR_W;
    params.height = TASKMGR_H;
    window_id = user_gui_create_window(&params);
    if (window_id < 0) return 1;
    (void)user_gui_set_title(window_id, "Task Manager");
    if (resize_surface(window_id) != 0) return 1;
    refresh_data();

    while (running) {
        gui_window_event_t event;
        uint32_t now = user_uptime_ticks();

        while (user_gui_poll_event(window_id, &event) > 0) {
            if (event.type == GUI_WIN_EVT_CLOSE_REQUEST) {
                running = 0;
            } else if (event.type == GUI_WIN_EVT_WINDOW_RESIZED) {
                (void)resize_surface(window_id);
                dirty = 1;
            } else if (event.type == GUI_WIN_EVT_MOUSE_DOWN) {
                int row = (event.arg1 - TASKMGR_HEADER_H - 24) / TASKMGR_ROW_H;
                int idx = scroll_row + row;

                if (row >= 0 && idx >= 0 && idx < proc_count) {
                    selected_pid = procs[idx].pid;
                    dirty = 1;
                }
            } else if (event.type == GUI_WIN_EVT_MOUSE_WHEEL) {
                scroll_row += event.arg2 > 0 ? -1 : 1;
                dirty = 1;
            } else if (event.type == GUI_WIN_EVT_KEY_DOWN) {
                if (event.arg0 == 0x7F && selected_pid > 0 && selected_pid != user_getpid()) {
                    (void)user_kill(selected_pid);
                    refresh_data();
                    dirty = 1;
                } else if (event.arg0 == 'r' || event.arg0 == 'R') {
                    refresh_data();
                    dirty = 1;
                }
            } else if (event.type == GUI_WIN_EVT_PAINT || event.type == GUI_WIN_EVT_FOCUS_GAINED) {
                dirty = 1;
            }
        }
        if (now - last_refresh >= 50U) {
            refresh_data();
            last_refresh = now;
            dirty = 1;
        }
        if (dirty) {
            render_taskmgr();
            (void)present_surface(window_id);
            dirty = 0;
        }
        user_sleep(2);
    }
    (void)user_gui_destroy_window(window_id);
    if (surface) user_free(surface);
    return 0;
}
