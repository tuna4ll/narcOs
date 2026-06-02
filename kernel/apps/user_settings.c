#include <stdint.h>
#include "usermode.h"
#include "user_abi.h"
#include "user_gui_lib.h"

#define USER_CODE __attribute__((section(".user_code")))

#define SETTINGS_PAD 14
#define SETTINGS_GAP 12
#define SETTINGS_RADIUS UI_RADIUS_MD
#define SETTINGS_PRESET_COUNT ((int)(sizeof(settings_presets) / sizeof(settings_presets[0])))

typedef struct {
    int x;
    int y;
    int w;
    int h;
} settings_rect_t;

typedef struct {
    int offset;
    const char* label;
} settings_preset_t;

static const settings_preset_t settings_presets[] = {
    {-480, "UTC-8"},
    {-300, "UTC-5"},
    {0, "UTC"},
    {180, "UTC+3"},
    {330, "UTC+5:30"},
    {540, "UTC+9"}
};

enum {
    SETTINGS_HIT_NONE = 0,
    SETTINGS_HIT_DEC,
    SETTINGS_HIT_INC,
    SETTINGS_HIT_SAVE,
    SETTINGS_HIT_PRESET_BASE = 100
};

static USER_CODE int settings_dequeue_event(user_settings_state_t* state, int* out_type, int* out_value) {
    int head;

    if (!state || !out_type || !out_value) return 0;
    if (state->event_head == state->event_tail) return 0;
    head = state->event_head;
    *out_type = state->event_type[head];
    *out_value = state->event_arg[head];
    state->event_head = (head + 1) % USER_GUI_EVENT_QUEUE_CAP;
    return 1;
}

static USER_CODE int text_len(const char* text) {
    int len = 0;
    if (!text) return 0;
    while (text[len] != '\0') len++;
    return len;
}

static USER_CODE void copy_text_local(char* dst, int dst_len, const char* src) {
    int i = 0;
    if (!dst || dst_len <= 0) return;
    if (!src) src = "";
    while (src[i] != '\0' && i + 1 < dst_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static USER_CODE void format_two_digit(char* out, int value) {
    out[0] = (char)('0' + ((value / 10) % 10));
    out[1] = (char)('0' + (value % 10));
}

static USER_CODE void format_timezone(char* out, int out_len, int offset_minutes) {
    int abs_minutes;
    int hours;
    int minutes;
    int pos = 0;

    if (!out || out_len < 6) return;
    out[pos++] = 'U';
    out[pos++] = 'T';
    out[pos++] = 'C';
    if (offset_minutes == 0) {
        out[pos] = '\0';
        return;
    }
    out[pos++] = offset_minutes < 0 ? '-' : '+';
    abs_minutes = offset_minutes < 0 ? -offset_minutes : offset_minutes;
    hours = abs_minutes / 60;
    minutes = abs_minutes % 60;
    if (hours >= 10 && pos + 1 < out_len) out[pos++] = (char)('0' + (hours / 10));
    if (pos + 1 < out_len) out[pos++] = (char)('0' + (hours % 10));
    if (minutes != 0 && pos + 3 < out_len) {
        out[pos++] = ':';
        out[pos++] = (char)('0' + (minutes / 10));
        out[pos++] = (char)('0' + (minutes % 10));
    }
    out[pos] = '\0';
}

static USER_CODE settings_rect_t make_rect(int x, int y, int w, int h) {
    settings_rect_t rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    return rect;
}

static USER_CODE int point_in_rect(int px, int py, settings_rect_t rect) {
    return px >= rect.x && py >= rect.y && px < rect.x + rect.w && py < rect.y + rect.h;
}

static USER_CODE int settings_min_int(int a, int b) {
    return a < b ? a : b;
}

static USER_CODE int settings_max_int(int a, int b) {
    return a > b ? a : b;
}

static USER_CODE int settings_is_compact(int w, int h) {
    return w < 500 || h < 390;
}

static USER_CODE int settings_two_column(int w, int h) {
    return w >= 500 && h >= 390;
}

static USER_CODE int settings_preset_columns(int w, int h) {
    if (w < 390 || h < 340) return 2;
    return 3;
}

static USER_CODE uint32_t mix(uint32_t fg, uint32_t bg, int alpha) {
    return user_gui_mix_color(fg, bg, alpha);
}

static USER_CODE void fill_gradient(user_gui_surface_t* surface, int x, int y, int w, int h,
                                    uint32_t top, uint32_t bottom) {
    if (!surface || w <= 0 || h <= 0) return;
    for (int py = 0; py < h; py++) {
        int alpha = h > 1 ? (py * 255) / (h - 1) : 255;
        user_gui_fill_rect(surface, x, y + py, w, 1, mix(bottom, top, alpha));
    }
}

static USER_CODE void draw_panel(user_gui_surface_t* surface, settings_rect_t rect,
                                 uint32_t top, uint32_t bottom, uint32_t border) {
    if (!surface || rect.w <= 0 || rect.h <= 0) return;
    user_gui_draw_rounded_rect(surface, rect.x + 2, rect.y + 3, rect.w, rect.h, SETTINGS_RADIUS, UI_SHADOW, 58);
    user_gui_draw_rounded_rect(surface, rect.x, rect.y, rect.w, rect.h, SETTINGS_RADIUS, bottom, 255);
    fill_gradient(surface, rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, top, bottom);
    user_gui_draw_rounded_rect(surface, rect.x, rect.y, rect.w, rect.h, SETTINGS_RADIUS, border, 255);
    if (rect.w > 24) user_gui_fill_rect_alpha(surface, rect.x + 12, rect.y + 1, rect.w - 24, 1, UI_HILITE_SOFT, 36);
}

static USER_CODE void draw_string_scaled(user_gui_surface_t* surface, int x, int y,
                                         const char* text, int scale, uint32_t color) {
    if (!surface || !text || scale <= 0) return;
    if (scale >= 2) {
        user_gui_draw_string_tall(surface, x, y, text, color);
        return;
    }
    while (*text) {
        user_gui_draw_char(surface, x, y, *text, color);
        x += 8;
        text++;
    }
}

static USER_CODE int scaled_text_width(const char* text, int scale) {
    if (scale >= 2) return text_len(text) * 9;
    return text_len(text) * 8;
}

static USER_CODE int scaled_text_height(int scale) {
    return scale >= 2 ? 18 : 10;
}

static USER_CODE void copy_text_ellipsized_local(char* dst, int dst_len, const char* src, int max_px) {
    int max_chars;
    int src_len;
    int i;

    if (!dst || dst_len <= 0) return;
    if (!src) src = "";
    if (max_px <= 0) {
        dst[0] = '\0';
        return;
    }
    max_chars = max_px / 8;
    if (max_chars < 1) {
        dst[0] = '\0';
        return;
    }
    if (max_chars > dst_len - 1) max_chars = dst_len - 1;
    src_len = text_len(src);
    if (src_len <= max_chars) {
        copy_text_local(dst, dst_len, src);
        return;
    }
    if (max_chars <= 3) {
        for (i = 0; i < max_chars && i + 1 < dst_len; i++) dst[i] = '.';
        dst[i] = '\0';
        return;
    }
    for (i = 0; i < max_chars - 3 && i + 1 < dst_len; i++) dst[i] = src[i];
    dst[i++] = '.';
    dst[i++] = '.';
    dst[i++] = '.';
    dst[i] = '\0';
}

static USER_CODE void draw_text_fit(user_gui_surface_t* surface, int x, int y, int max_w,
                                    const char* text, uint32_t color) {
    char clipped[80];

    if (!surface || max_w <= 0) return;
    copy_text_ellipsized_local(clipped, (int)sizeof(clipped), text, max_w);
    user_gui_draw_string(surface, x, y, clipped, color);
}

static USER_CODE void draw_button(user_gui_surface_t* surface, settings_rect_t rect, const char* label,
                                  uint32_t top, uint32_t bottom, uint32_t border, uint32_t text, int scale) {
    int tx;
    int ty;
    int label_w;
    char clipped[48];

    draw_panel(surface, rect, top, bottom, border);
    if (scale < 1) scale = 1;
    copy_text_ellipsized_local(clipped, (int)sizeof(clipped), label, rect.w - 16);
    label_w = scaled_text_width(clipped, scale);
    tx = rect.x + (rect.w - label_w) / 2;
    if (tx < rect.x + 8) tx = rect.x + 8;
    ty = rect.y + (rect.h - scaled_text_height(scale)) / 2;
    if (scale == 1) user_gui_draw_string(surface, tx, ty, clipped, text);
    else draw_string_scaled(surface, tx, ty, clipped, scale, text);
}

static USER_CODE settings_rect_t settings_header_rect(int w, int h) {
    int compact = settings_is_compact(w, h);
    (void)h;
    return make_rect(SETTINGS_PAD, SETTINGS_PAD, w - SETTINGS_PAD * 2, compact ? 54 : 58);
}

static USER_CODE settings_rect_t settings_value_rect(int w, int h) {
    settings_rect_t header = settings_header_rect(w, h);
    int y = header.y + header.h + SETTINGS_GAP;
    if (settings_two_column(w, h)) {
        int col_w = (header.w - SETTINGS_GAP) / 2;
        return make_rect(header.x, y, col_w, 152);
    }
    return make_rect(header.x, y, header.w, settings_is_compact(w, h) ? 124 : 136);
}

static USER_CODE settings_rect_t settings_status_rect(int w, int h) {
    settings_rect_t header = settings_header_rect(w, h);
    settings_rect_t value = settings_value_rect(w, h);
    if (settings_two_column(w, h)) {
        return make_rect(value.x + value.w + SETTINGS_GAP, value.y,
                         header.x + header.w - (value.x + value.w + SETTINGS_GAP), value.h);
    }
    return make_rect(value.x, value.y + value.h + SETTINGS_GAP, value.w,
                     settings_is_compact(w, h) ? 76 : 84);
}

static USER_CODE settings_rect_t settings_presets_rect(int w, int h) {
    settings_rect_t status = settings_status_rect(w, h);
    int columns = settings_preset_columns(w, h);
    int rows = (SETTINGS_PRESET_COUNT + columns - 1) / columns;
    int chip_h = settings_is_compact(w, h) ? 28 : 30;
    int spacing_y = 8;
    int content_h = rows * chip_h + (rows - 1) * spacing_y;
    int preset_h = 36 + content_h + 12;
    int y = status.y + status.h + SETTINGS_GAP;
    int max_h = h - y - SETTINGS_PAD - 54;

    if (max_h > 58 && preset_h > max_h) preset_h = max_h;
    if (preset_h < 58) preset_h = 58;
    return make_rect(status.x, y, status.w, preset_h);
}

static USER_CODE settings_rect_t settings_footer_rect(int w, int h) {
    settings_rect_t presets = settings_presets_rect(w, h);
    int y = presets.y + presets.h + SETTINGS_GAP;
    int footer_h = h - y - SETTINGS_PAD;
    if (footer_h < 42) footer_h = 42;
    return make_rect(presets.x, y, presets.w, footer_h);
}

static USER_CODE settings_rect_t settings_minus_rect(int w, int h) {
    settings_rect_t value = settings_value_rect(w, h);
    int compact = settings_is_compact(w, h);
    int button_w = compact ? 40 : 44;
    int button_h = 34;
    int gap = compact ? 8 : 12;
    return make_rect(value.x + value.w - (button_w * 2 + gap + 14),
                     value.y + value.h - button_h - 14,
                     button_w, button_h);
}

static USER_CODE settings_rect_t settings_plus_rect(int w, int h) {
    settings_rect_t minus = settings_minus_rect(w, h);
    return make_rect(minus.x + minus.w + (settings_is_compact(w, h) ? 8 : 12), minus.y, minus.w, minus.h);
}

static USER_CODE settings_rect_t settings_save_rect(int w, int h) {
    settings_rect_t footer = settings_footer_rect(w, h);
    int compact = settings_is_compact(w, h);
    int button_w = compact ? settings_min_int(128, footer.w - 24) : settings_min_int(128, footer.w - 24);
    if (button_w < 92) button_w = 92;
    return make_rect(footer.x + footer.w - button_w - 12,
                     footer.y + (footer.h - 30) / 2,
                     button_w, 30);
}

static USER_CODE settings_rect_t settings_preset_rect(int index, int width, int height) {
    settings_rect_t presets = settings_presets_rect(width, height);
    int columns = settings_preset_columns(width, height);
    int compact = settings_is_compact(width, height);
    int gap_x = compact ? 8 : 10;
    int gap_y = 8;
    int chip_h = compact ? 28 : 30;
    int inner_w = presets.w - 24;
    int chip_w = (inner_w - (columns - 1) * gap_x) / columns;
    int row = index / columns;
    int col = index % columns;
    int y = presets.y + 32 + row * (chip_h + gap_y);

    chip_w = settings_max_int(chip_w, 72);
    if (y + chip_h > presets.y + presets.h - 8) return make_rect(0, 0, 0, 0);
    return make_rect(presets.x + 12 + col * (chip_w + gap_x),
                     y,
                     chip_w, chip_h);
}

static USER_CODE int settings_hit_test(user_settings_state_t* state, int px, int py) {
    user_gui_surface_t surface;

    if (!state) return SETTINGS_HIT_NONE;
    surface.width = state->render_w > 0 ? state->render_w : USER_SETTINGS_SURFACE_W;
    surface.height = state->render_h > 0 ? state->render_h : USER_SETTINGS_SURFACE_H;

    if (point_in_rect(px, py, settings_minus_rect(surface.width, surface.height))) return SETTINGS_HIT_DEC;
    if (point_in_rect(px, py, settings_plus_rect(surface.width, surface.height))) return SETTINGS_HIT_INC;
    if (point_in_rect(px, py, settings_save_rect(surface.width, surface.height))) return SETTINGS_HIT_SAVE;
    for (int i = 0; i < (int)(sizeof(settings_presets) / sizeof(settings_presets[0])); i++) {
        if (point_in_rect(px, py, settings_preset_rect(i, surface.width, surface.height))) return SETTINGS_HIT_PRESET_BASE + i;
    }
    return SETTINGS_HIT_NONE;
}

static USER_CODE void settings_apply_hit(int hit) {
    if (hit == SETTINGS_HIT_DEC) {
        (void)user_set_timezone_offset_minutes(user_get_timezone_offset_minutes() - 30);
    } else if (hit == SETTINGS_HIT_INC) {
        (void)user_set_timezone_offset_minutes(user_get_timezone_offset_minutes() + 30);
    } else if (hit == SETTINGS_HIT_SAVE) {
        (void)user_save_timezone_setting();
        return;
    } else if (hit >= SETTINGS_HIT_PRESET_BASE) {
        int preset_idx = hit - SETTINGS_HIT_PRESET_BASE;
        if (preset_idx >= 0 && preset_idx < (int)(sizeof(settings_presets) / sizeof(settings_presets[0]))) {
            (void)user_set_timezone_offset_minutes(settings_presets[preset_idx].offset);
        }
    } else {
        return;
    }
    (void)user_save_timezone_setting();
}

static USER_CODE void settings_handle_key(int scancode) {
    switch (scancode) {
        case 0x4B:
        case 0x0C:
            settings_apply_hit(SETTINGS_HIT_DEC);
            break;
        case 0x4D:
        case 0x0D:
        case 0x4E:
            settings_apply_hit(SETTINGS_HIT_INC);
            break;
        case 0x02: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 0); break;
        case 0x03: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 1); break;
        case 0x04: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 2); break;
        case 0x05: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 3); break;
        case 0x06: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 4); break;
        case 0x07: settings_apply_hit(SETTINGS_HIT_PRESET_BASE + 5); break;
        case 0x1C:
            settings_apply_hit(SETTINGS_HIT_SAVE);
            break;
        default:
            break;
    }
}

static USER_CODE void settings_render(user_settings_state_t* state) {
    user_gui_surface_t surface;
    rtc_local_time_t now;
    net_ipv4_config_t config;
    char title_time[9];
    char date_str[11];
    char tz_str[16];
    char network_str[32];
    char hint_str[64];
    char footer_hint_str[32];
    char status_str[16];
    int width;
    int height;
    int offset;
    int compact;
    settings_rect_t header;
    settings_rect_t value;
    settings_rect_t status;
    settings_rect_t presets;
    settings_rect_t footer;
    settings_rect_t save;
    int two_col;

    if (!state || !state->surface) return;
    if (user_get_local_time(&now) != 0) return;

    surface.pixels = state->surface;
    surface.width = state->render_w > 0 ? state->render_w : USER_SETTINGS_SURFACE_W;
    surface.height = state->render_h > 0 ? state->render_h : USER_SETTINGS_SURFACE_H;
    width = surface.width;
    height = surface.height;
    compact = settings_is_compact(width, height);
    two_col = settings_two_column(width, height);
    offset = user_get_timezone_offset_minutes();
    header = settings_header_rect(width, height);
    value = settings_value_rect(width, height);
    status = settings_status_rect(width, height);
    presets = settings_presets_rect(width, height);
    footer = settings_footer_rect(width, height);
    save = settings_save_rect(width, height);

    format_two_digit(&title_time[0], now.hour);
    title_time[2] = ':';
    format_two_digit(&title_time[3], now.minute);
    title_time[5] = ':';
    format_two_digit(&title_time[6], now.second);
    title_time[8] = '\0';

    date_str[0] = (char)('0' + ((now.year / 1000) % 10));
    date_str[1] = (char)('0' + ((now.year / 100) % 10));
    date_str[2] = (char)('0' + ((now.year / 10) % 10));
    date_str[3] = (char)('0' + (now.year % 10));
    date_str[4] = '-';
    format_two_digit(&date_str[5], now.month);
    date_str[7] = '-';
    format_two_digit(&date_str[8], now.day);
    date_str[10] = '\0';

    format_timezone(tz_str, sizeof(tz_str), offset);
    if (user_net_get_config(&config) == 0 && config.available) {
        copy_text_local(network_str, sizeof(network_str), config.configured ? "Network ready" : "Waiting for DHCP");
        copy_text_local(status_str, sizeof(status_str), config.configured ? "ONLINE" : "DHCP");
    } else {
        copy_text_local(network_str, sizeof(network_str), "Network offline");
        copy_text_local(status_str, sizeof(status_str), "OFFLINE");
    }
    copy_text_local(hint_str, sizeof(hint_str),
                    compact ? "Left/right adjusts, 1-6 selects" : "Left/right adjusts timezone. Keys 1-6 select presets.");
    copy_text_local(footer_hint_str, sizeof(footer_hint_str), "Changes are saved instantly");

    fill_gradient(&surface, 0, 0, width, height, 0x101821, 0x0B1117);
    user_gui_fill_rect_alpha(&surface, 0, 0, width, height, UI_ACCENT_DEEP, 10);

    draw_panel(&surface, header, 0x1A2530, 0x101820, UI_BORDER_SOFT);
    user_gui_draw_rounded_rect(&surface, header.x + 12, header.y + 12, 30, 30, UI_RADIUS_SM,
                               mix(UI_ACCENT_ALT, UI_SURFACE_0, 96), 235);
    user_gui_draw_icon(&surface, USER_GUI_ICON_SETTINGS, header.x + 18, header.y + 18, 18, UI_ACCENT_ALT, 1);
    user_gui_draw_string(&surface, header.x + 54, header.y + 14, "Settings", UI_TEXT);
    draw_text_fit(&surface, header.x + 54, header.y + 30, header.w - 180, date_str, UI_TEXT_SUBTLE);
    draw_button(&surface, make_rect(header.x + header.w - 100, header.y + 15, 86, 28),
                status_str, status_str[0] == 'O' && status_str[1] == 'N' ? 0x356957 : 0x303945,
                status_str[0] == 'O' && status_str[1] == 'N' ? 0x1D4D3C : 0x1B232C,
                status_str[0] == 'O' && status_str[1] == 'N' ? UI_SUCCESS : UI_BORDER_SOFT,
                UI_TEXT, 1);

    draw_panel(&surface, value, 0x17212B, 0x10171E, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, value.x + 14, value.y + 14, "Time zone", UI_TEXT_SUBTLE);
    draw_string_scaled(&surface, value.x + 14, value.y + 33, tz_str, compact ? 2 : 2, UI_ACCENT_ALT);
    user_gui_draw_string(&surface, value.x + 14, value.y + 70, "Local time", UI_TEXT_SUBTLE);
    draw_string_scaled(&surface, value.x + 14, value.y + 88, title_time, compact ? 1 : 2, UI_TEXT);
    draw_button(&surface, settings_minus_rect(width, height), "-", 0x253140, 0x151E28, UI_BORDER_SOFT, UI_TEXT, 2);
    draw_button(&surface, settings_plus_rect(width, height), "+", 0x253140, 0x151E28, UI_BORDER_SOFT, UI_TEXT, 2);

    draw_panel(&surface, status, 0x17212B, 0x10171E, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, status.x + 14, status.y + 14, "System", UI_TEXT_SUBTLE);
    user_gui_draw_icon(&surface, USER_GUI_ICON_INFO, status.x + 14, status.y + 36, 22, UI_ACCENT_ALT, 0);
    draw_text_fit(&surface, status.x + 46, status.y + 39, status.w - 60, network_str, UI_TEXT);
    user_gui_draw_string(&surface, status.x + 14, status.y + (two_col ? 78 : 58), "Date", UI_TEXT_SUBTLE);
    draw_text_fit(&surface, status.x + 14, status.y + (two_col ? 96 : 72), status.w - 28, date_str, UI_TEXT_MUTED);
    if (two_col) {
        user_gui_draw_string(&surface, status.x + 14, status.y + 120, "Profile", UI_TEXT_SUBTLE);
        draw_text_fit(&surface, status.x + 78, status.y + 120, status.w - 92, "Desktop settings", UI_TEXT_MUTED);
    }

    draw_panel(&surface, presets, 0x17212B, 0x10171E, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, presets.x + 14, presets.y + 12, "Timezone presets", UI_TEXT_SUBTLE);
    for (int i = 0; i < SETTINGS_PRESET_COUNT; i++) {
        settings_rect_t rect = settings_preset_rect(i, width, height);
        int active = settings_presets[i].offset == offset;
        if (rect.w <= 0 || rect.h <= 0) continue;
        draw_button(&surface, rect, settings_presets[i].label,
                    active ? 0x3F7B68 : 0x253140,
                    active ? 0x23604B : 0x151E28,
                    active ? UI_SUCCESS : UI_BORDER_SOFT,
                    active ? UI_TEXT : UI_TEXT_MUTED, 1);
    }

    draw_panel(&surface, footer, 0x161F29, 0x0F161D, UI_BORDER_SOFT);
    draw_text_fit(&surface, footer.x + 14, footer.y + 9, save.x - footer.x - 28, hint_str, UI_TEXT_MUTED);
    draw_text_fit(&surface, footer.x + 14, footer.y + 23, save.x - footer.x - 28, footer_hint_str, UI_TEXT_SUBTLE);
    draw_button(&surface, save, "Save", 0x3F7B68, 0x23604B, UI_SUCCESS, UI_TEXT, 1);
}

void USER_CODE user_settings_entry_c(user_settings_state_t* state) {
    if (!state) return;

    for (;;) {
        int event_type;
        int event_value;
        int needs_render = state->dirty != 0;

        while (settings_dequeue_event(state, &event_type, &event_value)) {
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
                    settings_apply_hit(settings_hit_test(state,
                                                         USER_SETTINGS_POINT_X(event_value),
                                                         USER_SETTINGS_POINT_Y(event_value)));
                    break;
                case USER_SETTINGS_EVT_KEY_DOWN:
                    settings_handle_key(event_value);
                    break;
                default:
                    break;
            }
            state->dirty = 1;
            needs_render = 1;
        }
        if (needs_render) {
            settings_render(state);
            state->dirty = 0;
        }
        user_yield();
    }
}
