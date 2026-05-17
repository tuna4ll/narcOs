#include <stdint.h>
#include "string.h"
#include "usermode.h"
#include "user_abi.h"
#include "user_gui_lib.h"

#define USER_CODE __attribute__((section(".user_code")))

#include "user_string.h"

#define strlen user_strlen
#define strncpy user_strncpy
#define memset user_memset

#define NARCPAD_MARGIN 12
#define NARCPAD_HEADER_H 42
#define NARCPAD_STATUS_H 24
#define NARCPAD_GUTTER_W 44
#define NARCPAD_TEXT_PAD_X 12
#define NARCPAD_TEXT_PAD_Y 10
#define NARCPAD_LINE_H 16
#define NARCPAD_CHAR_W 8
#define NARCPAD_TAB_W 4
#define NARCPAD_MIN_WRAP_COLS 8
#define NARCPAD_LINE_BUF 192

typedef struct {
    int editor_x;
    int editor_y;
    int editor_w;
    int editor_h;
    int gutter_x;
    int gutter_w;
    int text_x;
    int text_y;
    int text_w;
    int text_h;
    int chars_per_line;
    int visible_lines;
} narcpad_layout_t;

typedef struct {
    int total_visual_lines;
    int caret_visual_line;
    int caret_col;
    int physical_lines;
    int char_count;
} narcpad_text_measure_t;

static USER_CODE int narcpad_dequeue_event(user_narcpad_state_t* state, int* out_type, int* out_value) {
    int head;

    if (!state || !out_type || !out_value) return 0;
    if (state->event_head == state->event_tail) return 0;
    head = state->event_head;
    *out_type = state->event_type[head];
    *out_value = state->event_arg[head];
    state->event_head = (head + 1) % USER_GUI_EVENT_QUEUE_CAP;
    return 1;
}

static USER_CODE void narcpad_copy_title_from_path(user_narcpad_state_t* state, const char* path) {
    const char* name = path;

    if (!state || !path || path[0] == '\0') return;
    while (*path) {
        if (*path == '/' && path[1] != '\0') name = path + 1;
        path++;
    }
    strncpy(state->title, name, sizeof(state->title) - 1U);
    state->title[sizeof(state->title) - 1U] = '\0';
}

static USER_CODE void narcpad_open_new(user_narcpad_state_t* state) {
    if (!state || !state->surface) return;
    state->path[0] = '\0';
    state->request_path[0] = '\0';
    state->content[0] = '\0';
    state->view_scroll = -1;
    strncpy(state->title, "untitled.txt", sizeof(state->title) - 1U);
    state->title[sizeof(state->title) - 1U] = '\0';
    state->dirty = 1;
}

static USER_CODE void narcpad_open_path(user_narcpad_state_t* state) {
    if (!state || state->request_path[0] == '\0') return;
    strncpy(state->path, state->request_path, sizeof(state->path) - 1U);
    state->path[sizeof(state->path) - 1U] = '\0';
    if (user_fs_read(state->path, state->content, sizeof(state->content)) != 0) {
        state->content[0] = '\0';
    }
    state->view_scroll = -1;
    narcpad_copy_title_from_path(state, state->path);
    state->request_path[0] = '\0';
    state->dirty = 1;
}

static USER_CODE void narcpad_save(user_narcpad_state_t* state) {
    const char* target;

    if (!state) return;
    target = state->path[0] != '\0' ? state->path : state->title;
    if (!target || target[0] == '\0') return;
    if (user_fs_write(target, state->content) == 0 && state->path[0] == '\0') {
        strncpy(state->path, target, sizeof(state->path) - 1U);
        state->path[sizeof(state->path) - 1U] = '\0';
    }
    state->dirty = 1;
}

static USER_CODE void narcpad_backspace(user_narcpad_state_t* state) {
    uint32_t len;

    if (!state) return;
    len = (uint32_t)strlen(state->content);
    if (len == 0U) return;
    state->content[len - 1U] = '\0';
    state->view_scroll = -1;
    state->dirty = 1;
}

static USER_CODE void narcpad_append_char(user_narcpad_state_t* state, char c) {
    uint32_t len;

    if (!state || c == '\0') return;
    len = (uint32_t)strlen(state->content);
    if (len + 1U >= sizeof(state->content)) return;
    state->content[len] = c;
    state->content[len + 1U] = '\0';
    state->view_scroll = -1;
    state->dirty = 1;
}

static USER_CODE void narcpad_append_newline(user_narcpad_state_t* state) {
    narcpad_append_char(state, '\n');
}

static USER_CODE void narcpad_draw_panel_flat(user_gui_surface_t* surface, int x, int y, int w, int h,
                                              int radius, uint32_t fill, int fill_alpha,
                                              uint32_t border, int border_alpha) {
    user_gui_draw_rounded_rect(surface, x, y, w, h, radius, fill, fill_alpha);
    if (w > 6 && h > 6) {
        user_gui_draw_rounded_rect(surface, x + 1, y + 1, w - 2, h - 2,
                                   radius > 1 ? radius - 1 : radius, 0xFFFFFF, 5);
    }
    user_gui_draw_rounded_rect(surface, x, y, w, h, radius, border, border_alpha);
}

static USER_CODE int narcpad_text_len(const char* text) {
    int len = 0;

    if (!text) return 0;
    while (text[len] != '\0') len++;
    return len;
}

static USER_CODE void narcpad_init_layout(narcpad_layout_t* layout, int surface_w, int surface_h) {
    int header_h = NARCPAD_HEADER_H;
    int status_h = NARCPAD_STATUS_H;

    if (!layout) return;
    if (surface_w < 1) surface_w = USER_NARCPAD_SURFACE_W;
    if (surface_h < 1) surface_h = USER_NARCPAD_SURFACE_H;
    if (surface_h < 180) {
        header_h = 0;
        status_h = 0;
    }
    layout->editor_x = surface_w > 96 ? NARCPAD_MARGIN : 0;
    layout->editor_y = header_h > 0 ? header_h : 0;
    layout->editor_w = surface_w - layout->editor_x * 2;
    layout->editor_h = surface_h - layout->editor_y - status_h - NARCPAD_MARGIN;
    if (layout->editor_w < 96) {
        layout->editor_x = 0;
        layout->editor_w = surface_w;
    }
    if (layout->editor_h < 48) {
        layout->editor_y = 0;
        layout->editor_h = surface_h;
    }
    layout->gutter_x = layout->editor_x;
    layout->gutter_w = surface_w < 420 ? 34 : NARCPAD_GUTTER_W;
    layout->text_x = layout->editor_x + layout->gutter_w + NARCPAD_TEXT_PAD_X;
    layout->text_y = layout->editor_y + NARCPAD_TEXT_PAD_Y;
    layout->text_w = layout->editor_w - layout->gutter_w - NARCPAD_TEXT_PAD_X * 2 - 8;
    layout->text_h = layout->editor_h - NARCPAD_TEXT_PAD_Y * 2;
    if (layout->text_w < NARCPAD_MIN_WRAP_COLS * NARCPAD_CHAR_W) layout->text_w = NARCPAD_MIN_WRAP_COLS * NARCPAD_CHAR_W;
    if (layout->text_h < NARCPAD_LINE_H) layout->text_h = NARCPAD_LINE_H;
    layout->chars_per_line = layout->text_w / NARCPAD_CHAR_W;
    if (layout->chars_per_line < NARCPAD_MIN_WRAP_COLS) layout->chars_per_line = NARCPAD_MIN_WRAP_COLS;
    if (layout->chars_per_line >= NARCPAD_LINE_BUF) layout->chars_per_line = NARCPAD_LINE_BUF - 1;
    layout->visible_lines = layout->text_h / NARCPAD_LINE_H;
    if (layout->visible_lines < 1) layout->visible_lines = 1;
}

static USER_CODE int narcpad_is_space(char c) {
    return c == ' ' || c == '\t';
}

static USER_CODE int narcpad_is_wrap_break(char c) {
    return c == ' ' || c == '\t' || c == '-' || c == '/' || c == '.' || c == ',';
}

static USER_CODE int narcpad_wrap_segment(const char* line, int line_len, int offset,
                                          int cols, int* out_next_offset) {
    int remaining;
    int break_pos = -1;
    int seg_len;
    int next_offset;

    if (!line || !out_next_offset || cols < 1) return 0;
    remaining = line_len - offset;
    if (remaining <= 0) {
        *out_next_offset = offset;
        return 0;
    }
    if (remaining <= cols) {
        *out_next_offset = line_len;
        return remaining;
    }
    for (int i = cols; i >= NARCPAD_MIN_WRAP_COLS; i--) {
        if (narcpad_is_wrap_break(line[offset + i - 1])) {
            break_pos = i;
            break;
        }
    }
    if (break_pos < 1) {
        *out_next_offset = offset + cols;
        return cols;
    }
    seg_len = break_pos;
    next_offset = offset + break_pos;
    if (narcpad_is_space(line[offset + break_pos - 1])) {
        seg_len = break_pos - 1;
        while (seg_len > 0 && narcpad_is_space(line[offset + seg_len - 1])) seg_len--;
        while (next_offset < line_len && narcpad_is_space(line[next_offset])) next_offset++;
        if (seg_len < 1) {
            seg_len = break_pos;
            next_offset = offset + break_pos;
        }
    }
    if (next_offset <= offset) next_offset = offset + seg_len;
    if (next_offset <= offset) next_offset = offset + 1;
    *out_next_offset = next_offset;
    return seg_len;
}

static USER_CODE int narcpad_segment_to_buffer(const char* text, int len, char* out, int out_len, int max_cols) {
    int col = 0;
    int pos = 0;

    if (!out || out_len <= 0) return 0;
    if (!text) text = "";
    for (int i = 0; i < len && pos + 1 < out_len && col < max_cols; i++) {
        char c = text[i];

        if (c == '\t') {
            int spaces = NARCPAD_TAB_W - (col % NARCPAD_TAB_W);

            if (spaces < 1) spaces = 1;
            while (spaces-- > 0 && pos + 1 < out_len && col < max_cols) {
                out[pos++] = ' ';
                col++;
            }
        } else {
            if ((unsigned char)c < 32U) c = '?';
            out[pos++] = c;
            col++;
        }
    }
    out[pos] = '\0';
    return col;
}

static USER_CODE int narcpad_segment_display_width(const char* text, int len, int max_cols) {
    char scratch[NARCPAD_LINE_BUF];

    return narcpad_segment_to_buffer(text, len, scratch, sizeof(scratch), max_cols);
}

static USER_CODE int narcpad_digit_count(int value) {
    int digits = 1;

    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

static USER_CODE void narcpad_draw_segment(user_gui_surface_t* surface, const narcpad_layout_t* layout,
                                           const narcpad_text_measure_t* measure, int top_line,
                                           int visual_line, int physical_line, int first_wrap,
                                           const char* text, int len) {
    char line_buf[NARCPAD_LINE_BUF];
    int row_y;
    int col_count;

    if (!surface || !layout || visual_line < top_line ||
        visual_line >= top_line + layout->visible_lines) {
        return;
    }
    row_y = layout->text_y + (visual_line - top_line) * NARCPAD_LINE_H;
    if (measure && visual_line == measure->caret_visual_line) {
        user_gui_fill_rect(surface, layout->editor_x + 1, row_y,
                           layout->editor_w - 2, NARCPAD_LINE_H, 0xE8F1FA);
    }
    if (first_wrap) {
        int digits = narcpad_digit_count(physical_line);
        int tx = layout->gutter_x + layout->gutter_w - digits * NARCPAD_CHAR_W - 8;

        if (tx < layout->gutter_x + 2) tx = layout->gutter_x + 2;
        user_gui_draw_int_crisp_tall(surface, tx, row_y, physical_line, UI_TEXT_SUBTLE);
    } else {
        user_gui_draw_string_crisp_tall(surface, layout->gutter_x + layout->gutter_w - 18, row_y,
                                        ">", UI_TEXT_SUBTLE);
    }
    col_count = narcpad_segment_to_buffer(text, len, line_buf, sizeof(line_buf), layout->chars_per_line);
    (void)col_count;
    user_gui_draw_string_crisp_tall(surface, layout->text_x, row_y, line_buf, UI_TEXT_DARK);
}

static USER_CODE void narcpad_walk_text(const char* content, const narcpad_layout_t* layout,
                                        int top_line, user_gui_surface_t* surface,
                                        const narcpad_text_measure_t* draw_measure,
                                        narcpad_text_measure_t* out_measure) {
    const char* p = content ? content : "";
    int visual_line = 0;
    int physical_line = 1;
    int char_count = 0;
    int keep_walking = 1;
    int caret_col = 0;
    int caret_visual_line = 0;

    while (keep_walking) {
        const char* line = p;
        int line_len = 0;
        int offset = 0;
        int first_wrap = 1;

        while (line[line_len] != '\0' && line[line_len] != '\n') line_len++;
        char_count += line_len;
        if (line_len == 0) {
            if (surface) {
                narcpad_draw_segment(surface, layout, draw_measure, top_line, visual_line,
                                     physical_line, 1, "", 0);
            }
            caret_visual_line = visual_line;
            caret_col = 0;
            visual_line++;
        } else {
            while (offset < line_len) {
                int next_offset = offset;
                int seg_len = narcpad_wrap_segment(line, line_len, offset,
                                                   layout->chars_per_line, &next_offset);

                if (surface) {
                    narcpad_draw_segment(surface, layout, draw_measure, top_line, visual_line,
                                         physical_line, first_wrap, line + offset, seg_len);
                }
                caret_visual_line = visual_line;
                caret_col = narcpad_segment_display_width(line + offset, seg_len, layout->chars_per_line);
                visual_line++;
                first_wrap = 0;
                offset = next_offset;
            }
        }
        if (line[line_len] == '\n') {
            char_count++;
            p = line + line_len + 1;
            physical_line++;
        } else {
            keep_walking = 0;
        }
    }
    if (out_measure) {
        out_measure->total_visual_lines = visual_line > 0 ? visual_line : 1;
        out_measure->caret_visual_line = caret_visual_line;
        out_measure->caret_col = caret_col;
        out_measure->physical_lines = physical_line;
        out_measure->char_count = char_count;
    }
}

static USER_CODE void narcpad_draw_header(user_gui_surface_t* surface, user_narcpad_state_t* state,
                                          const narcpad_text_measure_t* measure) {
    const char* path;
    int title_len;
    int chip_x;

    if (!surface || surface->height < 180) return;
    user_gui_fill_rect(surface, 0, 0, surface->width, NARCPAD_HEADER_H, UI_SURFACE_0);
    user_gui_fill_rect_alpha(surface, 0, NARCPAD_HEADER_H - 1, surface->width, 1, UI_ACCENT_ALT, 92);
    user_gui_draw_string_tall_shadow(surface, 16, 7, "NarcPad", UI_TEXT, UI_SHADOW);
    user_gui_fill_rect_alpha(surface, 86, 11, 1, 18, UI_BORDER_SOFT, 255);
    user_gui_draw_string_crisp(surface, 96, 10, state && state->title[0] ? state->title : "untitled.txt", UI_TEXT);
    path = state && state->path[0] ? state->path : "Unsaved document";
    user_gui_draw_string_crisp(surface, 96, 25, path, UI_TEXT_MUTED);
    title_len = state && state->title[0] ? narcpad_text_len(state->title) : 12;
    chip_x = 112 + title_len * NARCPAD_CHAR_W;
    if (chip_x < surface->width - 110 && measure) {
        narcpad_draw_panel_flat(surface, chip_x, 9, 82, 20, UI_RADIUS_SM,
                                UI_SURFACE_2, 235, UI_BORDER_SOFT, 255);
        user_gui_draw_int_crisp(surface, chip_x + 10, 15, measure->char_count, UI_ACCENT_ALT);
        user_gui_draw_string_crisp(surface, chip_x + 40, 15, "ch", UI_TEXT_MUTED);
    }
}

static USER_CODE void narcpad_draw_status(user_gui_surface_t* surface, const narcpad_layout_t* layout,
                                          const narcpad_text_measure_t* measure, int top_line) {
    int y;
    int x;

    if (!surface || !layout || !measure || surface->height < 180) return;
    y = surface->height - NARCPAD_STATUS_H;
    user_gui_fill_rect(surface, 0, y, surface->width, NARCPAD_STATUS_H, UI_SURFACE_0);
    user_gui_fill_rect_alpha(surface, 0, y, surface->width, 1, UI_BORDER_SOFT, 255);
    x = 16;
    user_gui_draw_string_crisp(surface, x, y + 8, "Ln", UI_TEXT_SUBTLE);
    user_gui_draw_int_crisp(surface, x + 24, y + 8, measure->physical_lines, UI_TEXT);
    x += 78;
    user_gui_draw_string_crisp(surface, x, y + 8, "Col", UI_TEXT_SUBTLE);
    user_gui_draw_int_crisp(surface, x + 32, y + 8, measure->caret_col + 1, UI_TEXT);
    x += 92;
    user_gui_draw_string_crisp(surface, x, y + 8, "Rows", UI_TEXT_SUBTLE);
    user_gui_draw_int_crisp(surface, x + 42, y + 8, measure->total_visual_lines, UI_TEXT);
    x += 112;
    user_gui_draw_string_crisp(surface, x, y + 8, "Top", UI_TEXT_SUBTLE);
    user_gui_draw_int_crisp(surface, x + 34, y + 8, top_line + 1, UI_TEXT_MUTED);
}

static USER_CODE void narcpad_draw_scrollbar(user_gui_surface_t* surface, const narcpad_layout_t* layout,
                                             const narcpad_text_measure_t* measure, int top_line) {
    int max_top;
    int track_x;
    int track_y;
    int track_h;
    int thumb_h;
    int thumb_y;

    if (!surface || !layout || !measure) return;
    max_top = measure->total_visual_lines > layout->visible_lines ?
              measure->total_visual_lines - layout->visible_lines : 0;
    if (max_top <= 0) return;
    track_x = layout->editor_x + layout->editor_w - 8;
    track_y = layout->text_y;
    track_h = layout->text_h;
    thumb_h = (layout->visible_lines * track_h) / measure->total_visual_lines;
    if (thumb_h < 18) thumb_h = 18;
    if (thumb_h > track_h) thumb_h = track_h;
    thumb_y = track_y + (top_line * (track_h - thumb_h)) / max_top;
    user_gui_draw_rounded_rect(surface, track_x, track_y, 3, track_h, 2, 0xD2DAE3, 255);
    user_gui_draw_rounded_rect(surface, track_x - 1, thumb_y, 5, thumb_h, 3, UI_ACCENT_DEEP, 255);
}

static USER_CODE void narcpad_scroll_by(user_narcpad_state_t* state, int wheel_steps) {
    narcpad_layout_t layout;
    narcpad_text_measure_t measure;
    int surface_w;
    int surface_h;
    int max_top;
    int top_line;

    if (!state || wheel_steps == 0) return;
    surface_w = state->render_w > 0 ? state->render_w : USER_NARCPAD_SURFACE_W;
    surface_h = state->render_h > 0 ? state->render_h : USER_NARCPAD_SURFACE_H;
    narcpad_init_layout(&layout, surface_w, surface_h);
    narcpad_walk_text(state->content, &layout, 0, 0, 0, &measure);
    max_top = measure.total_visual_lines > layout.visible_lines ?
              measure.total_visual_lines - layout.visible_lines : 0;
    top_line = state->view_scroll;
    if (top_line < 0) top_line = max_top;
    top_line -= wheel_steps * 3;
    if (top_line < 0) top_line = 0;
    if (top_line >= max_top) state->view_scroll = -1;
    else state->view_scroll = top_line;
    state->dirty = 1;
}

static USER_CODE void narcpad_render(user_narcpad_state_t* state) {
    user_gui_surface_t surface;
    narcpad_layout_t layout;
    narcpad_text_measure_t measure;
    int max_top_line;
    int top_line;
    int blink_phase;

    if (!state) return;
    surface.pixels = state->surface;
    surface.width = state->render_w > 0 ? state->render_w : USER_NARCPAD_SURFACE_W;
    surface.height = state->render_h > 0 ? state->render_h : USER_NARCPAD_SURFACE_H;
    if (!surface.pixels || surface.width <= 0 || surface.height <= 0) return;

    narcpad_init_layout(&layout, surface.width, surface.height);
    narcpad_walk_text(state->content, &layout, 0, 0, 0, &measure);
    max_top_line = measure.total_visual_lines > layout.visible_lines ?
                   measure.total_visual_lines - layout.visible_lines : 0;
    if (state->view_scroll < 0) state->view_scroll = max_top_line;
    if (state->view_scroll > max_top_line) state->view_scroll = max_top_line;
    top_line = state->view_scroll;

    user_gui_fill_rect(&surface, 0, 0, surface.width, surface.height, UI_SURFACE_0);
    narcpad_draw_header(&surface, state, &measure);
    narcpad_draw_panel_flat(&surface, layout.editor_x, layout.editor_y, layout.editor_w, layout.editor_h,
                            UI_RADIUS_MD, 0xF6F8FB, 255, 0xCFD8E2, 255);
    user_gui_fill_rect(&surface, layout.editor_x + 1, layout.editor_y + 1,
                       layout.gutter_w - 2, layout.editor_h - 2, 0xEDF2F7);
    user_gui_fill_rect_alpha(&surface, layout.editor_x + layout.gutter_w, layout.editor_y + 8,
                             1, layout.editor_h - 16, 0xCFD8E2, 255);
    narcpad_walk_text(state->content, &layout, top_line, &surface, &measure, 0);
    blink_phase = (int)((user_uptime_ticks() / 20U) & 1U);
    if (blink_phase == 0 &&
        measure.caret_visual_line >= top_line &&
        measure.caret_visual_line < top_line + layout.visible_lines) {
        int caret_x = layout.text_x + measure.caret_col * NARCPAD_CHAR_W;
        int caret_y = layout.text_y + (measure.caret_visual_line - top_line) * NARCPAD_LINE_H;
        int max_caret_x = layout.text_x + layout.text_w - 2;

        if (caret_x > max_caret_x) caret_x = max_caret_x;
        user_gui_fill_rect(&surface, caret_x, caret_y, 2, NARCPAD_LINE_H, UI_ACCENT_DEEP);
    }
    narcpad_draw_scrollbar(&surface, &layout, &measure, top_line);
    narcpad_draw_status(&surface, &layout, &measure, top_line);
    state->last_blink_phase = blink_phase;
}

void USER_CODE user_narcpad_entry_c(user_narcpad_state_t* state) {
    if (!state) return;
    if (state->title[0] == '\0') narcpad_open_new(state);

    for (;;) {
        int event_type;
        int event_value;
        int blink_phase = (int)((user_uptime_ticks() / 20U) & 1U);
        int needs_render = state->dirty != 0 || state->last_blink_phase != blink_phase;

        while (narcpad_dequeue_event(state, &event_type, &event_value)) {
            switch (event_type) {
                case USER_NARCPAD_EVT_CHAR:
                    narcpad_append_char(state, (char)event_value);
                    break;
                case USER_NARCPAD_EVT_BACKSPACE:
                    narcpad_backspace(state);
                    break;
                case USER_NARCPAD_EVT_NEWLINE:
                    narcpad_append_newline(state);
                    break;
                case USER_NARCPAD_EVT_SAVE:
                    narcpad_save(state);
                    break;
                case USER_NARCPAD_EVT_OPEN_NEW:
                    narcpad_open_new(state);
                    break;
                case USER_NARCPAD_EVT_OPEN_PATH:
                    narcpad_open_path(state);
                    break;
                case USER_NARCPAD_EVT_SCROLL:
                    narcpad_scroll_by(state, event_value);
                    break;
                default:
                    break;
            }
            needs_render = 1;
        }
        if (needs_render) {
            narcpad_render(state);
            state->dirty = 0;
        }
        user_yield();
    }
}
