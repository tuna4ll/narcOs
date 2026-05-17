#include <stdint.h>
#include "usermode.h"
#include "user_abi.h"
#include "user_gui_lib.h"

#define USER_CODE __attribute__((section(".user_code")))

#define SETTINGS_PAD 16
#define SETTINGS_GAP 12
#define SETTINGS_RADIUS 10

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
    return w < 440 || h < 360;
}

static USER_CODE int settings_preset_columns(int w) {
    return w < 380 ? 1 : 2;
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
    user_gui_draw_rounded_rect(surface, rect.x, rect.y, rect.w, rect.h, SETTINGS_RADIUS, bottom, 255);
    fill_gradient(surface, rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, top, bottom);
    user_gui_draw_rounded_rect(surface, rect.x, rect.y, rect.w, rect.h, SETTINGS_RADIUS, border, 255);
}

static USER_CODE void draw_char_scaled(user_gui_surface_t* surface, int x, int y, char c, int scale, uint32_t color) {
    const unsigned char* glyph;

    if (!surface || scale <= 0) return;
    glyph = user_gui_font[(uint8_t)c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((glyph[row] & (1U << (7 - col))) == 0U) continue;
            user_gui_fill_rect(surface, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static USER_CODE void draw_string_scaled(user_gui_surface_t* surface, int x, int y,
                                         const char* text, int scale, uint32_t color) {
    if (!surface || !text || scale <= 0) return;
    while (*text) {
        draw_char_scaled(surface, x, y, *text, scale, color);
        x += 8 * scale;
        text++;
    }
}

static USER_CODE void draw_button(user_gui_surface_t* surface, settings_rect_t rect, const char* label,
                                  uint32_t top, uint32_t bottom, uint32_t border, uint32_t text, int scale) {
    int tx;
    int ty;
    int label_w;

    draw_panel(surface, rect, top, bottom, border);
    label_w = text_len(label) * 8 * scale;
    tx = rect.x + (rect.w - label_w) / 2;
    if (tx < rect.x + 8) tx = rect.x + 8;
    ty = rect.y + (rect.h - (8 * scale)) / 2;
    draw_string_scaled(surface, tx, ty, label, scale, text);
}

static USER_CODE settings_rect_t settings_header_rect(int w, int h) {
    int compact = settings_is_compact(w, h);
    return make_rect(SETTINGS_PAD, SETTINGS_PAD, w - SETTINGS_PAD * 2, compact ? 74 : 86);
}

static USER_CODE settings_rect_t settings_value_rect(int w, int h) {
    settings_rect_t header = settings_header_rect(w, h);
    return make_rect(header.x, header.y + header.h + SETTINGS_GAP, header.w,
                     settings_is_compact(w, h) ? 98 : 110);
}

static USER_CODE settings_rect_t settings_presets_rect(int w, int h) {
    settings_rect_t value = settings_value_rect(w, h);
    int columns = settings_preset_columns(w);
    int rows = ((int)(sizeof(settings_presets) / sizeof(settings_presets[0])) + columns - 1) / columns;
    int chip_h = settings_is_compact(w, h) ? 24 : 22;
    int spacing_y = 10;
    int content_h = rows * chip_h + (rows - 1) * spacing_y;
    return make_rect(value.x, value.y + value.h + SETTINGS_GAP, value.w, 34 + content_h + 14);
}

static USER_CODE settings_rect_t settings_footer_rect(int w, int h) {
    settings_rect_t presets = settings_presets_rect(w, h);
    int y = presets.y + presets.h + SETTINGS_GAP;
    int footer_h = h - y - SETTINGS_PAD;
    if (footer_h < 52) footer_h = 52;
    return make_rect(presets.x, y, presets.w, footer_h);
}

static USER_CODE settings_rect_t settings_minus_rect(int w, int h) {
    settings_rect_t value = settings_value_rect(w, h);
    int compact = settings_is_compact(w, h);
    int button_w = compact ? 38 : 44;
    int button_h = compact ? 32 : 38;
    int gap = compact ? 8 : 12;
    return make_rect(value.x + value.w - (button_w * 2 + gap + 16),
                     value.y + (compact ? 18 : 26),
                     button_w, button_h);
}

static USER_CODE settings_rect_t settings_plus_rect(int w, int h) {
    settings_rect_t minus = settings_minus_rect(w, h);
    return make_rect(minus.x + minus.w + (settings_is_compact(w, h) ? 8 : 12), minus.y, minus.w, minus.h);
}

static USER_CODE settings_rect_t settings_save_rect(int w, int h) {
    settings_rect_t footer = settings_footer_rect(w, h);
    int compact = settings_is_compact(w, h);
    int button_w = compact ? footer.w - 32 : settings_min_int(132, footer.w - 24);
    if (button_w < 92) button_w = 92;
    return make_rect(compact ? footer.x + 16 : footer.x + footer.w - button_w - 16,
                     footer.y + (footer.h - 34) / 2,
                     button_w, 34);
}

static USER_CODE settings_rect_t settings_preset_rect(int index, int width, int height) {
    settings_rect_t presets = settings_presets_rect(width, height);
    int columns = settings_preset_columns(width);
    int compact = settings_is_compact(width, height);
    int gap_x = compact ? 10 : 16;
    int gap_y = 10;
    int chip_h = compact ? 24 : 22;
    int inner_w = presets.w - 28;
    int chip_w = (inner_w - (columns - 1) * gap_x) / columns;
    int row = index / columns;
    int col = index % columns;

    chip_w = settings_max_int(chip_w, 92);
    return make_rect(presets.x + 14 + col * (chip_w + gap_x),
                     presets.y + 32 + row * (chip_h + gap_y),
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
    char hint_str[48];
    char footer_hint_str[24];
    char status_str[16];
    int width;
    int height;
    int offset;
    int compact;
    settings_rect_t header;
    settings_rect_t value;
    settings_rect_t presets;
    settings_rect_t footer;

    if (!state || !state->surface) return;
    if (user_get_local_time(&now) != 0) return;

    surface.pixels = state->surface;
    surface.width = state->render_w > 0 ? state->render_w : USER_SETTINGS_SURFACE_W;
    surface.height = state->render_h > 0 ? state->render_h : USER_SETTINGS_SURFACE_H;
    width = surface.width;
    height = surface.height;
    compact = settings_is_compact(width, height);
    offset = user_get_timezone_offset_minutes();
    header = settings_header_rect(width, height);
    value = settings_value_rect(width, height);
    presets = settings_presets_rect(width, height);
    footer = settings_footer_rect(width, height);

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
        copy_text_local(network_str, sizeof(network_str), config.configured ? "Network ready" : "Waiting DHCP");
        copy_text_local(status_str, sizeof(status_str), config.configured ? "ONLINE" : "DHCP");
    } else {
        copy_text_local(network_str, sizeof(network_str), "Network offline");
        copy_text_local(status_str, sizeof(status_str), "OFFLINE");
    }
    copy_text_local(hint_str, sizeof(hint_str),
                    compact ? "ARROWS adjust  1-6 preset" : "LEFT/RIGHT adjust  1-6 preset  ENTER save");
    copy_text_local(footer_hint_str, sizeof(footer_hint_str), compact ? "ENTER saves changes" : "Saved instantly");

    fill_gradient(&surface, 0, 0, width, height, 0x0D1218, 0x131B24);
    user_gui_fill_rect_alpha(&surface, 0, 0, width, height, 0x071018, 22);

    draw_panel(&surface, make_rect(SETTINGS_PAD, SETTINGS_PAD, width - SETTINGS_PAD * 2, height - SETTINGS_PAD * 2),
               0x131B25, 0x0C1218, UI_BORDER_SOFT);

    draw_panel(&surface, header, 0x1B2632, 0x111821, UI_BORDER_SOFT);
    user_gui_draw_string_tall_shadow(&surface, header.x + 14, header.y + 12, "SETTINGS", UI_TEXT, UI_SHADOW);
    user_gui_draw_string(&surface, header.x + 16, header.y + (compact ? 46 : 52), date_str, UI_TEXT_MUTED);
    if (compact) {
        user_gui_draw_string(&surface, header.x + header.w - 96, header.y + 48, network_str, UI_TEXT_SUBTLE);
    } else {
        user_gui_draw_string(&surface, header.x + 120, header.y + 52, network_str, UI_TEXT_SUBTLE);
    }
    draw_button(&surface, make_rect(header.x + header.w - (compact ? 98 : 106), header.y + 22,
                                    compact ? 82 : 90, 28),
                status_str, 0x264C46, 0x16352F, UI_ACCENT_ALT, UI_TEXT, 1);

    draw_panel(&surface, value, 0x161F29, 0x0F161D, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, value.x + 16, value.y + 14, "Current time", UI_TEXT_SUBTLE);
    draw_string_scaled(&surface, value.x + 16, value.y + 30, title_time, compact ? 2 : 3, UI_TEXT);
    user_gui_draw_string(&surface, value.x + 16, value.y + (compact ? 62 : 80), "Timezone", UI_TEXT_SUBTLE);
    draw_string_scaled(&surface, value.x + 16, value.y + (compact ? 72 : 92), tz_str, 2, UI_ACCENT_ALT);
    draw_button(&surface, settings_minus_rect(width, height), "-", 0x1B2430, 0x111922, UI_BORDER_SOFT, UI_TEXT, 2);
    draw_button(&surface, settings_plus_rect(width, height), "+", 0x1B2430, 0x111922, UI_BORDER_SOFT, UI_TEXT, 2);

    draw_panel(&surface, presets, 0x161F29, 0x0F161D, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, presets.x + 16, presets.y + 12, "Presets", UI_TEXT_SUBTLE);
    for (int i = 0; i < (int)(sizeof(settings_presets) / sizeof(settings_presets[0])); i++) {
        settings_rect_t rect = settings_preset_rect(i, width, height);
        int active = settings_presets[i].offset == offset;
        draw_button(&surface, rect, settings_presets[i].label,
                    active ? 0x46786B : 0x1B2430,
                    active ? UI_ACCENT_DEEP : 0x111922,
                    active ? UI_ACCENT_ALT : UI_BORDER_SOFT,
                    active ? UI_TEXT_DARK : UI_TEXT, 1);
    }

    draw_panel(&surface, footer, 0x151D26, 0x0F151C, UI_BORDER_SOFT);
    user_gui_draw_string(&surface, footer.x + 16, footer.y + 10, hint_str, UI_TEXT_MUTED);
    user_gui_draw_string(&surface, footer.x + 16, footer.y + 24, footer_hint_str, UI_TEXT_SUBTLE);
    draw_button(&surface, settings_save_rect(width, height), "SAVE", 0x46786B, UI_ACCENT_DEEP, UI_ACCENT_ALT, UI_TEXT_DARK, 1);
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
