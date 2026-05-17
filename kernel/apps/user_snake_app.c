#include <stdint.h>

#include "usermode.h"
#include "user_abi.h"
#include "ui_theme.h"

#define USER_CODE __attribute__((section(".user_code")))

#define UI_GLYPH_W       7
#define UI_GLYPH_ADVANCE 7

static USER_CODE uint32_t snake_mix_color(uint32_t c1, uint32_t c2, int alpha) {
    uint32_t rb1;
    uint32_t g1;
    uint32_t rb2;
    uint32_t g2;
    uint32_t rb;
    uint32_t g;

    if (alpha >= 255) return c1;
    if (alpha <= 0) return c2;

    rb1 = c1 & 0xFF00FFU;
    g1 = c1 & 0x00FF00U;
    rb2 = c2 & 0xFF00FFU;
    g2 = c2 & 0x00FF00U;
    rb = ((rb1 * (uint32_t)alpha + rb2 * (uint32_t)(256 - alpha)) >> 8) & 0xFF00FFU;
    g = ((g1 * (uint32_t)alpha + g2 * (uint32_t)(256 - alpha)) >> 8) & 0x00FF00U;
    return rb | g;
}

static USER_CODE int snake_surface_w(user_snake_state_t* state) {
    if (!state || state->render_w <= 0) return USER_SNAKE_SURFACE_W;
    return state->render_w;
}

static USER_CODE int snake_surface_h(user_snake_state_t* state) {
    if (!state || state->render_h <= 0) return USER_SNAKE_SURFACE_H;
    return state->render_h;
}

static USER_CODE uint32_t* snake_surface_pixels(user_snake_state_t* state) {
    if (!state) return 0;
    return state->surface_ptr ? state->surface_ptr : state->surface;
}

static USER_CODE void snake_put_pixel(user_snake_state_t* state, int x, int y, uint32_t color) {
    uint32_t* pixels = snake_surface_pixels(state);
    int width = snake_surface_w(state);
    int height = snake_surface_h(state);

    if (!pixels) return;
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    pixels[y * width + x] = color;
}

static USER_CODE uint32_t snake_get_pixel(user_snake_state_t* state, int x, int y) {
    uint32_t* pixels = snake_surface_pixels(state);
    int width = snake_surface_w(state);
    int height = snake_surface_h(state);

    if (!pixels) return 0;
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    return pixels[y * width + x];
}

static USER_CODE void snake_put_pixel_alpha(user_snake_state_t* state, int x, int y, uint32_t color, int alpha) {
    uint32_t old_color;

    if (!state || alpha <= 0) return;
    if (alpha >= 255) {
        snake_put_pixel(state, x, y, color);
        return;
    }

    old_color = snake_get_pixel(state, x, y);
    snake_put_pixel(state, x, y, snake_mix_color(color, old_color, alpha));
}

static USER_CODE void snake_fill_rect(user_snake_state_t* state, int x, int y, int w, int h, uint32_t color) {
    uint32_t* pixels = snake_surface_pixels(state);
    int surface_w = snake_surface_w(state);
    int surface_h = snake_surface_h(state);

    if (!pixels || w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > surface_w) w = surface_w - x;
    if (y + h > surface_h) h = surface_h - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            pixels[yy * surface_w + xx] = color;
        }
    }
}

static USER_CODE void snake_fill_rect_alpha(user_snake_state_t* state, int x, int y, int w, int h, uint32_t color, int alpha) {
    uint32_t* pixels = snake_surface_pixels(state);
    int surface_w = snake_surface_w(state);
    int surface_h = snake_surface_h(state);

    if (!pixels || alpha <= 0) return;
    if (alpha >= 255) {
        snake_fill_rect(state, x, y, w, h, color);
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > surface_w) w = surface_w - x;
    if (y + h > surface_h) h = surface_h - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            uint32_t old_color = pixels[yy * surface_w + xx];
            pixels[yy * surface_w + xx] = snake_mix_color(color, old_color, alpha);
        }
    }
}

static USER_CODE int snake_get_aa_alpha(int dx, int dy, int r, int base_alpha) {
    int r2 = r * r;
    int d2 = dx * dx + dy * dy;
    int diff_r2;
    int dist_pct;

    if (d2 <= (r - 1) * (r - 1)) return base_alpha;
    if (d2 > r2) return 0;
    diff_r2 = r2 - (r - 1) * (r - 1);
    if (diff_r2 <= 0) return base_alpha;
    dist_pct = (r2 - d2) * 255 / diff_r2;
    return (base_alpha * dist_pct) >> 8;
}

static USER_CODE void snake_draw_rounded_rect(user_snake_state_t* state, int x, int y, int w, int h, int r, uint32_t color, int alpha) {
    if (!state || w <= 0 || h <= 0) return;
    if (r <= 0) {
        snake_fill_rect_alpha(state, x, y, w, h, color, alpha);
        return;
    }

    snake_fill_rect_alpha(state, x + r, y, w - 2 * r, h, color, alpha);
    snake_fill_rect_alpha(state, x, y + r, r, h - 2 * r, color, alpha);
    snake_fill_rect_alpha(state, x + w - r, y + r, r, h - 2 * r, color, alpha);

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < r; j++) {
            snake_put_pixel_alpha(state, x + j, y + i, color, snake_get_aa_alpha(r - j, r - i, r, alpha));
            snake_put_pixel_alpha(state, x + w - r + j, y + i, color, snake_get_aa_alpha(j + 1, r - i, r, alpha));
            snake_put_pixel_alpha(state, x + j, y + h - r + i, color, snake_get_aa_alpha(r - j, i + 1, r, alpha));
            snake_put_pixel_alpha(state, x + w - r + j, y + h - r + i, color, snake_get_aa_alpha(j + 1, i + 1, r, alpha));
        }
    }
}

static USER_CODE void snake_draw_rect(user_snake_state_t* state, int x, int y, int w, int h, uint32_t color) {
    snake_fill_rect(state, x, y, w, 1, color);
    snake_fill_rect(state, x, y + h - 1, w, 1, color);
    snake_fill_rect(state, x, y, 1, h, color);
    snake_fill_rect(state, x + w - 1, y, 1, h, color);
}

static USER_CODE const unsigned char* snake_get_glyph(char c) {
    static const struct {
        char c;
        unsigned char rows[8];
    } glyphs[] = {
        {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {'0', {0x00,0x30,0x48,0x68,0x78,0x58,0x48,0x30}},
        {'1', {0x00,0x30,0x70,0x20,0x20,0x20,0x20,0x78}},
        {'2', {0x00,0x70,0x48,0x08,0x70,0x40,0x40,0x78}},
        {'3', {0x00,0x78,0x08,0x18,0x30,0x08,0x48,0x70}},
        {'4', {0x00,0x18,0x38,0x28,0x48,0x78,0x08,0x08}},
        {'5', {0x00,0x78,0x40,0x70,0x08,0x08,0x48,0x70}},
        {'6', {0x00,0x18,0x20,0x40,0x70,0x48,0x48,0x30}},
        {'7', {0x00,0x78,0x48,0x08,0x18,0x10,0x20,0x20}},
        {'8', {0x00,0x30,0x48,0x48,0x30,0x48,0x48,0x30}},
        {'9', {0x00,0x30,0x48,0x48,0x38,0x08,0x10,0x60}},
        {'A', {0x00,0x20,0x30,0x50,0x50,0xF8,0x88,0x88}},
        {'B', {0x00,0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0}},
        {'E', {0x00,0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8}},
        {'G', {0x00,0x70,0x88,0x80,0xB8,0x88,0x88,0x70}},
        {'K', {0x00,0x88,0x90,0xA0,0xE0,0x90,0x88,0x88}},
        {'M', {0x00,0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88}},
        {'O', {0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70}},
        {'P', {0x00,0xF0,0x88,0x88,0xF0,0x80,0x80,0x80}},
        {'R', {0x00,0xF0,0x88,0x88,0xF0,0xB0,0x98,0x88}},
        {'S', {0x00,0x70,0x88,0x80,0x70,0x08,0x88,0x70}},
        {'V', {0x00,0x88,0x88,0x88,0xD8,0x50,0x70,0x20}},
        {'a', {0x00,0x00,0x00,0x70,0x08,0x78,0x88,0x78}},
        {'c', {0x00,0x00,0x00,0x38,0x40,0x40,0x40,0x38}},
        {'e', {0x00,0x00,0x00,0x70,0x88,0xF8,0x80,0x78}},
        {'k', {0x00,0x80,0x80,0x98,0xA0,0xC0,0xA0,0x98}},
        {'n', {0x00,0x00,0x00,0xB0,0xC8,0x88,0x88,0x88}},
        {'o', {0x00,0x00,0x00,0x70,0x88,0x88,0x88,0x70}},
        {'r', {0x00,0x00,0x00,0xB8,0xC0,0x80,0x80,0x80}},
        {'s', {0x00,0x00,0x00,0x78,0x40,0x30,0x08,0xF0}},
        {'t', {0x00,0x40,0x40,0xF0,0x40,0x40,0x40,0x38}}
    };
    static const unsigned char fallback[8] = {0x00,0x70,0x48,0x18,0x10,0x00,0x00,0x10};

    for (unsigned int i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].c == c) return glyphs[i].rows;
    }
    return fallback;
}

static USER_CODE void snake_draw_char(user_snake_state_t* state, int x, int y, char c, uint32_t color) {
    const unsigned char* glyph = snake_get_glyph(c);

    if (!state) return;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < UI_GLYPH_W; col++) {
            if ((glyph[row] & (1U << (7 - col))) != 0U) {
                snake_put_pixel_alpha(state, x + col, y + row, color, 255);
            }
        }
    }
}

static USER_CODE void snake_draw_string(user_snake_state_t* state, int x, int y, const char* s, uint32_t color) {
    if (!state || !s) return;
    while (*s) {
        snake_draw_char(state, x, y, *s, color);
        x += UI_GLYPH_ADVANCE;
        s++;
    }
}

static USER_CODE void snake_draw_int(user_snake_state_t* state, int x, int y, int num, uint32_t color) {
    char buf[16];
    int pos = 0;
    int n = num;

    if (!state) return;
    if (n == 0) {
        buf[pos++] = '0';
    } else {
        if (n < 0) {
            snake_draw_char(state, x, y, '-', color);
            x += UI_GLYPH_ADVANCE;
            n = -n;
        }
        while (n > 0) {
            buf[pos++] = (char)((n % 10) + '0');
            n /= 10;
        }
    }

    for (int i = pos - 1; i >= 0; i--) {
        snake_draw_char(state, x, y, buf[i], color);
        x += UI_GLYPH_ADVANCE;
    }
}

static USER_CODE void snake_draw_panel_flat(user_snake_state_t* state, int x, int y, int w, int h, int radius,
                                            uint32_t fill, int fill_alpha, uint32_t border, int border_alpha) {
    snake_draw_rounded_rect(state, x, y, w, h, radius, fill, fill_alpha);
    if (w > 6 && h > 6) {
        snake_draw_rounded_rect(state, x + 1, y + 1, w - 2, h - 2,
                                radius > 1 ? radius - 1 : radius, 0xFFFFFFU, 5);
    }
    snake_draw_rounded_rect(state, x, y, w, h, radius, border, border_alpha);
}

static USER_CODE void snake_render(user_snake_state_t* state) {
    int x = 0;
    int y = 0;
    int w = snake_surface_w(state);
    int h = snake_surface_h(state);
    int header_x = x + 14;
    int header_y = y + 14;
    int header_w = w - 28;
    int header_h = 52;
    int grid_w = 39;
    int grid_h = 29;
    int cell_w = (w - 40) / grid_w;
    int cell_h = (h - 92) / grid_h;
    int cell = cell_w < cell_h ? cell_w : cell_h;
    int dot = cell - 2;
    int board_w;
    int board_h;
    int content_top = y + 66;
    int board_x;
    int board_y;
    int game_over_x = x + (w - 84) / 2;
    int reset_text_x = x + (w - 128) / 2;

    if (!state) return;
    if (cell > 9) cell = 9;
    if (cell < 5) cell = 5;
    if (dot < 3) dot = 3;
    board_w = grid_w * cell + 12;
    board_h = grid_h * cell + 12;
    board_x = x + (w - board_w) / 2;
    board_y = content_top + ((h - (content_top - y) - board_h) > 0 ? (h - (content_top - y) - board_h) / 2 : 0);
    if (board_x < x + 8) board_x = x + 8;
    if (board_y < content_top) board_y = content_top;

    snake_fill_rect(state, 0, 0, w, h, UI_SURFACE_0);
    snake_draw_panel_flat(state, x, y, w, h, UI_RADIUS_MD, UI_SURFACE_1, 235, UI_BORDER_SOFT, 255);
    snake_draw_panel_flat(state, header_x, header_y, header_w, header_h, UI_RADIUS_MD, UI_SURFACE_0, 255, UI_BORDER_SOFT, 255);
    snake_draw_string(state, header_x + 14, header_y + 12, "Snake", UI_TEXT);
    snake_draw_string(state, header_x + 14, header_y + 30, "Score", UI_TEXT_SUBTLE);
    snake_draw_int(state, header_x + 58, header_y + 30, state->score, UI_TEXT);
    snake_draw_string(state, header_x + 118, header_y + 30, "Best", UI_TEXT_SUBTLE);
    snake_draw_int(state, header_x + 154, header_y + 30, state->best, UI_TEXT);
    snake_draw_string(state, header_x + header_w - 58, header_y + 30, "R Reset", UI_TEXT_MUTED);

    snake_fill_rect_alpha(state, x + 14, header_y + header_h + 2, w - 28, 1, UI_BORDER_SOFT, 255);
    snake_fill_rect(state, board_x, board_y, board_w, board_h, 0x10171F);
    snake_draw_rect(state, board_x, board_y, board_w, board_h, UI_BORDER_SOFT);
    if (state->dead) {
        snake_draw_string(state, game_over_x, board_y + 112, "GAME OVER", UI_DANGER);
        snake_draw_string(state, reset_text_x, board_y + 134, "Press R to Reset", UI_TEXT_MUTED);
    } else {
        snake_fill_rect(state, board_x + 6 + state->apple_x * cell, board_y + 6 + state->apple_y * cell, dot, dot, UI_DANGER);
        for (int i = 0; i < state->len; i++) {
            uint32_t col = (i == 0) ? UI_SUCCESS : 0x37B24D;
            snake_fill_rect(state, board_x + 6 + state->px[i] * cell, board_y + 6 + state->py[i] * cell, dot, dot, col);
        }
    }
}

static USER_CODE void snake_spawn_apple(user_snake_state_t* state) {
    if (!state) return;
    state->apple_x = (int)(user_random_u32() % 37U) + 1;
    state->apple_y = (int)(user_random_u32() % 27U) + 1;
}

static USER_CODE void snake_reset(user_snake_state_t* state) {
    if (!state) return;

    state->len = 5;
    state->dead = 0;
    state->score = 0;
    state->dir = 3;
    state->last_tick = 0;

    for (int i = 0; i < 5; i++) {
        state->px[i] = 10 - i;
        state->py[i] = 10;
    }

    snake_spawn_apple(state);
    snake_render(state);
}

static USER_CODE int snake_direction_is_reverse(int current_dir, int next_dir) {
    return (current_dir == 0 && next_dir == 1) ||
           (current_dir == 1 && next_dir == 0) ||
           (current_dir == 2 && next_dir == 3) ||
           (current_dir == 3 && next_dir == 2);
}

static USER_CODE void snake_step(user_snake_state_t* state) {
    if (!state) return;

    for (int i = state->len - 1; i > 0; i--) {
        state->px[i] = state->px[i - 1];
        state->py[i] = state->py[i - 1];
    }

    if (state->dir == 0) state->py[0]--;
    else if (state->dir == 1) state->py[0]++;
    else if (state->dir == 2) state->px[0]--;
    else state->px[0]++;

    if (state->px[0] < 0 || state->px[0] >= 39 || state->py[0] < 0 || state->py[0] >= 29) {
        state->dead = 1;
        return;
    }

    for (int i = 1; i < state->len; i++) {
        if (state->px[0] == state->px[i] && state->py[0] == state->py[i]) {
            state->dead = 1;
            return;
        }
    }

    if (state->px[0] != state->apple_x || state->py[0] != state->apple_y) return;

    if (state->len < 100) state->len++;
    state->score += 10;
    if (state->score > state->best) state->best = state->score;
    snake_spawn_apple(state);
}

void USER_CODE user_snake_entry_c(user_snake_state_t* state) {
    if (!state) return;

    snake_reset(state);

    for (;;) {
        int input = user_snake_get_input();

        if (input == 6) {
            (void)user_snake_close();
            return;
        }
        if (input == 5) {
            snake_reset(state);
        } else if (input >= 0 && input <= 3 && !snake_direction_is_reverse(state->dir, input)) {
            state->dir = input;
        }

        if (!state->dead) {
            uint32_t now = user_uptime_ticks();

            if ((uint32_t)(now - (uint32_t)state->last_tick) > 10U) {
                state->last_tick = (int)now;
                snake_step(state);
            }
        }

        snake_render(state);
        user_yield();
    }
}
