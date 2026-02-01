/* Copyright (C) 2022-2025 mintsuki and contributors.
 * Copyright (C) 2026 iProgramInCpp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __cplusplus
#error "Please do not compile Flanterm as C++ code! Flanterm should be compiled as C99 or newer."
#endif

#ifndef __STDC_VERSION__
#error "Flanterm must be compiled as C99 or newer."
#endif

#if defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define ALWAYS_INLINE inline
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef FLANTERM_IN_FLANTERM
#define FLANTERM_IN_FLANTERM
#endif

#include "flanterm/src/flanterm.h"
#include "flanterm/src/flanterm_backends/fb.h"

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);

static ALWAYS_INLINE uint32_t convert_colour(struct flanterm_context *_ctx, uint32_t colour) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    uint32_t r = (colour >> 16) & 0xFF;
    uint32_t g = (colour >> 8) & 0xFF;
    uint32_t b = (colour) & 0xFF;

    r >>= (8 - ctx->red_mask_size);
    g >>= (8 - ctx->green_mask_size);
    b >>= (8 - ctx->blue_mask_size);

    return (r << ctx->red_mask_shift) | (g << ctx->green_mask_shift) | (b << ctx->blue_mask_shift);
}

static void flanterm_fb_save_state(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    ctx->saved_state_text_fg = ctx->text_fg;
    ctx->saved_state_text_bg = ctx->text_bg;
    ctx->saved_state_cursor_x = ctx->cursor_x;
    ctx->saved_state_cursor_y = ctx->cursor_y;
}

static void flanterm_fb_restore_state(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    ctx->text_fg = ctx->saved_state_text_fg;
    ctx->text_bg = ctx->saved_state_text_bg;
    ctx->cursor_x = ctx->saved_state_cursor_x;
    ctx->cursor_y = ctx->saved_state_cursor_y;
}

static void flanterm_fb_swap_palette(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    uint32_t tmp = ctx->text_bg;
    ctx->text_bg = ctx->text_fg;
    ctx->text_fg = tmp;
}

static void plot_char_unscaled_uncanvas_32(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    uint32_t default_bg = ctx->default_bg;

    uint32_t bg = c->bg == 0xffffffff ? default_bg : c->bg;
    uint32_t fg = c->fg == 0xffffffff ? default_bg : c->fg;

    x = ctx->offset_x + x * ctx->glyph_width;
    y = ctx->offset_y + y * ctx->glyph_height;

    bool *glyph = &ctx->font_bool[c->c * ctx->font_height * ctx->font_width];
    // naming: fx,fy for font coordinates, gx,gy for glyph coordinates
    for (size_t gy = 0; gy < ctx->glyph_height; gy++) {
        volatile uint32_t *fb_line = ctx->framebuffer + x + (y + gy) * (ctx->pitch / 4);
        bool *glyph_pointer = glyph + (gy * ctx->font_width);
        for (size_t fx = 0; fx < ctx->font_width; fx++) {
            fb_line[fx] = *(glyph_pointer++) ? fg : bg;
        }
    }
}

static void plot_char_unscaled_uncanvas_24(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    uint32_t default_bg = ctx->default_bg;

    uint32_t bg = c->bg == 0xffffffff ? default_bg : c->bg;
    uint32_t fg = c->fg == 0xffffffff ? default_bg : c->fg;

    x = ctx->offset_x + x * ctx->glyph_width;
    y = ctx->offset_y + y * ctx->glyph_height;

    bool *glyph = &ctx->font_bool[c->c * ctx->font_height * ctx->font_width];
    // naming: fx,fy for font coordinates, gx,gy for glyph coordinates
    volatile uint8_t *framebuffer8 = (volatile uint8_t *)ctx->framebuffer;
    for (size_t gy = 0; gy < ctx->glyph_height; gy++) {
        volatile uint8_t *fb_line = framebuffer8 + x * 3 + (y + gy) * ctx->pitch;
        bool *glyph_pointer = glyph + (gy * ctx->font_width);
        for (size_t fx = 0; fx < ctx->font_width; fx++) {
            uint32_t col = *(glyph_pointer++) ? fg : bg;
            *(fb_line++) = (uint8_t)(col);
            *(fb_line++) = (uint8_t)(col >> 8);
            *(fb_line++) = (uint8_t)(col >> 16);
        }
    }
}

static void plot_char_unscaled_uncanvas_16(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    uint32_t default_bg = ctx->default_bg;

    uint32_t bg = c->bg == 0xffffffff ? default_bg : c->bg;
    uint32_t fg = c->fg == 0xffffffff ? default_bg : c->fg;

    x = ctx->offset_x + x * ctx->glyph_width;
    y = ctx->offset_y + y * ctx->glyph_height;

    bool *glyph = &ctx->font_bool[c->c * ctx->font_height * ctx->font_width];
    // naming: fx,fy for font coordinates, gx,gy for glyph coordinates
    volatile uint16_t *framebuffer16 = (volatile uint16_t *)ctx->framebuffer;
    for (size_t gy = 0; gy < ctx->glyph_height; gy++) {
        volatile uint16_t *fb_line = framebuffer16 + x + (y + gy) * (ctx->pitch / 2);
        bool *glyph_pointer = glyph + (gy * ctx->font_width);
        for (size_t fx = 0; fx < ctx->font_width; fx++) {
            // note: colors were converted to 16-bit before this
            fb_line[fx] = *(glyph_pointer++) ? fg : bg;
        }
    }
}

static inline bool compare_char(struct flanterm_fb_char *a, struct flanterm_fb_char *b) {
    return !(a->c != b->c || a->bg != b->bg || a->fg != b->fg);
}

static void push_to_queue(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    size_t i = y * _ctx->cols + x;

    struct flanterm_fb_queue_item *q = ctx->map[i];

    if (q == NULL) {
        if (compare_char(&ctx->grid[i], c)) {
            return;
        }
        q = &ctx->queue[ctx->queue_i++];
        q->x = x;
        q->y = y;
        ctx->map[i] = q;
    }

    q->c = *c;
}

static void flanterm_fb_revscroll(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    for (size_t i = (_ctx->scroll_bottom_margin - 1) * _ctx->cols - 1;
         i >= _ctx->scroll_top_margin * _ctx->cols; i--) {
        if (i == (size_t)-1) {
            break;
        }
        struct flanterm_fb_char *c;
        struct flanterm_fb_queue_item *q = ctx->map[i];
        if (q != NULL) {
            c = &q->c;
        } else {
            c = &ctx->grid[i];
        }
        push_to_queue(_ctx, c, (i + _ctx->cols) % _ctx->cols, (i + _ctx->cols) / _ctx->cols);
    }

    // Clear the first line of the screen.
    struct flanterm_fb_char empty;
    empty.c  = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i, _ctx->scroll_top_margin);
    }
}

static void flanterm_fb_scroll(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    for (size_t i = (_ctx->scroll_top_margin + 1) * _ctx->cols;
         i < _ctx->scroll_bottom_margin * _ctx->cols; i++) {
        struct flanterm_fb_char *c;
        struct flanterm_fb_queue_item *q = ctx->map[i];
        if (q != NULL) {
            c = &q->c;
        } else {
            c = &ctx->grid[i];
        }
        push_to_queue(_ctx, c, (i - _ctx->cols) % _ctx->cols, (i - _ctx->cols) / _ctx->cols);
    }

    // Clear the last line of the screen.
    struct flanterm_fb_char empty;
    empty.c  = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i, _ctx->scroll_bottom_margin - 1);
    }
}

static void flanterm_fb_clear(struct flanterm_context *_ctx, bool move) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    struct flanterm_fb_char empty;
    empty.c  = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->rows * _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i % _ctx->cols, i / _ctx->cols);
    }

    if (move) {
        ctx->cursor_x = 0;
        ctx->cursor_y = 0;
    }
}

static void flanterm_fb_set_cursor_pos(struct flanterm_context *_ctx, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols) {
        if ((int)x < 0) {
            x = 0;
        } else {
            x = _ctx->cols - 1;
        }
    }
    if (y >= _ctx->rows) {
        if ((int)y < 0) {
            y = 0;
        } else {
            y = _ctx->rows - 1;
        }
    }
    ctx->cursor_x = x;
    ctx->cursor_y = y;
}

static void flanterm_fb_get_cursor_pos(struct flanterm_context *_ctx, size_t *x, size_t *y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    *x = ctx->cursor_x >= _ctx->cols ? _ctx->cols - 1 : ctx->cursor_x;
    *y = ctx->cursor_y >= _ctx->rows ? _ctx->rows - 1 : ctx->cursor_y;
}

static void flanterm_fb_move_character(struct flanterm_context *_ctx, size_t new_x, size_t new_y, size_t old_x, size_t old_y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (old_x >= _ctx->cols || old_y >= _ctx->rows
     || new_x >= _ctx->cols || new_y >= _ctx->rows) {
        return;
    }

    size_t i = old_x + old_y * _ctx->cols;

    struct flanterm_fb_char *c;
    struct flanterm_fb_queue_item *q = ctx->map[i];
    if (q != NULL) {
        c = &q->c;
    } else {
        c = &ctx->grid[i];
    }

    push_to_queue(_ctx, c, new_x, new_y);
}

static void flanterm_fb_set_text_fg(struct flanterm_context *_ctx, size_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->ansi_colours[fg];
}

static void flanterm_fb_set_text_bg(struct flanterm_context *_ctx, size_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->ansi_colours[bg];
}

static void flanterm_fb_set_text_fg_bright(struct flanterm_context *_ctx, size_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->ansi_bright_colours[fg];
}

static void flanterm_fb_set_text_bg_bright(struct flanterm_context *_ctx, size_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->ansi_bright_colours[bg];
}

static void flanterm_fb_set_text_fg_rgb(struct flanterm_context *_ctx, uint32_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = convert_colour(_ctx, fg);
}

static void flanterm_fb_set_text_bg_rgb(struct flanterm_context *_ctx, uint32_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = convert_colour(_ctx, bg);
}

static void flanterm_fb_set_text_fg_default(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->default_fg;
}

static void flanterm_fb_set_text_bg_default(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = 0xffffffff;
}

static void flanterm_fb_set_text_fg_default_bright(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->default_fg_bright;
}

static void flanterm_fb_set_text_bg_default_bright(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->default_bg_bright;
}

static void draw_cursor(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (ctx->cursor_x >= _ctx->cols || ctx->cursor_y >= _ctx->rows) {
        return;
    }

    size_t i = ctx->cursor_x + ctx->cursor_y * _ctx->cols;

    struct flanterm_fb_char c;
    struct flanterm_fb_queue_item *q = ctx->map[i];
    if (q != NULL) {
        c = q->c;
    } else {
        c = ctx->grid[i];
    }
    uint32_t tmp = c.fg;
    c.fg = c.bg;
    c.bg = tmp;
    ctx->plot_char(_ctx, &c, ctx->cursor_x, ctx->cursor_y);
    if (q != NULL) {
        ctx->grid[i] = q->c;
        ctx->map[i] = NULL;
    }
}

static void flanterm_fb_double_buffer_flush(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (_ctx->cursor_enabled) {
        draw_cursor(_ctx);
    }

    for (size_t i = 0; i < ctx->queue_i; i++) {
        struct flanterm_fb_queue_item *q = &ctx->queue[i];
        size_t offset = q->y * _ctx->cols + q->x;
        if (ctx->map[offset] == NULL) {
            continue;
        }
        ctx->plot_char(_ctx, &q->c, q->x, q->y);
        ctx->grid[offset] = q->c;
        ctx->map[offset] = NULL;
    }

    if ((ctx->old_cursor_x != ctx->cursor_x || ctx->old_cursor_y != ctx->cursor_y) || _ctx->cursor_enabled == false) {
        if (ctx->old_cursor_x < _ctx->cols && ctx->old_cursor_y < _ctx->rows) {
            ctx->plot_char(_ctx, &ctx->grid[ctx->old_cursor_x + ctx->old_cursor_y * _ctx->cols], ctx->old_cursor_x, ctx->old_cursor_y);
        }
    }

    ctx->old_cursor_x = ctx->cursor_x;
    ctx->old_cursor_y = ctx->cursor_y;

    ctx->queue_i = 0;
}

static void flanterm_fb_raw_putchar(struct flanterm_context *_ctx, uint8_t c) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (ctx->cursor_x >= _ctx->cols && (ctx->cursor_y < _ctx->scroll_bottom_margin - 1 || _ctx->scroll_enabled)) {
        ctx->cursor_x = 0;
        ctx->cursor_y++;
        if (ctx->cursor_y == _ctx->scroll_bottom_margin) {
            ctx->cursor_y--;
            flanterm_fb_scroll(_ctx);
        }
        if (ctx->cursor_y >= _ctx->cols) {
            ctx->cursor_y = _ctx->cols - 1;
        }
    }

    struct flanterm_fb_char ch;
    ch.c  = c;
    ch.fg = ctx->text_fg;
    ch.bg = ctx->text_bg;
    push_to_queue(_ctx, &ch, ctx->cursor_x++, ctx->cursor_y);
}

static void flanterm_fb_full_refresh(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    uint32_t default_bg = ctx->default_bg;

    switch (ctx->bpp) {
        case 32: {
            for (size_t y = 0; y < ctx->height; y++) {
                for (size_t x = 0; x < ctx->width; x++) {
                    ctx->framebuffer[y * (ctx->pitch / sizeof(uint32_t)) + x] = default_bg;
                }
            }
            break;
        }
        case 24: {
            volatile uint8_t *framebuffer8 = (volatile uint8_t *)ctx->framebuffer;
            for (size_t y = 0; y < ctx->height; y++) {
                volatile uint8_t *fb_line = framebuffer8 + y * ctx->pitch;
                for (size_t x = 0; x < ctx->width; x++) {
                    *(fb_line++) = (uint8_t)(default_bg);
                    *(fb_line++) = (uint8_t)(default_bg >> 8);
                    *(fb_line++) = (uint8_t)(default_bg >> 16);
                }
            }
            break;
        }
        case 16: {
            volatile uint16_t *framebuffer16 = (volatile uint16_t *)ctx->framebuffer;
            for (size_t y = 0; y < ctx->height; y++) {
                volatile uint16_t *fb_line = framebuffer16 + y * ctx->pitch / 2;
                for (size_t x = 0; x < ctx->width; x++) {
                    *(fb_line++) = (uint16_t) default_bg;
                }
            }
            break;
        }
    }

    for (size_t i = 0; i < (size_t)_ctx->rows * _ctx->cols; i++) {
        size_t x = i % _ctx->cols;
        size_t y = i / _ctx->cols;

        ctx->plot_char(_ctx, &ctx->grid[i], x, y);
    }

    if (_ctx->cursor_enabled) {
        draw_cursor(_ctx);
    }
}

static void flanterm_fb_deinit(struct flanterm_context *_ctx, void (*_free)(void *, size_t)) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (_free == NULL) {
#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
        if (bump_allocated_instance == true) {
            bump_alloc_ptr = 0;
            bump_allocated_instance = false;
        }
#endif
        return;
    }

    _free(ctx->font_bits, ctx->font_bits_size);
    _free(ctx->font_bool, ctx->font_bool_size);
    _free(ctx->grid, ctx->grid_size);
    _free(ctx->queue, ctx->queue_size);
    _free(ctx->map, ctx->map_size);

    _free(ctx, sizeof(struct flanterm_fb_context));
}

extern void DbgPrint(const char *fmt, ...);

struct flanterm_context *flanterm_fb_init_alt(
    void *(*_malloc)(size_t),
    void (*_free)(void *, size_t),
    uint32_t *framebuffer, size_t width, size_t height, size_t pitch,
    int bpp,
    uint8_t red_mask_size, uint8_t red_mask_shift,
    uint8_t green_mask_size, uint8_t green_mask_shift,
    uint8_t blue_mask_size, uint8_t blue_mask_shift,
    uint32_t *ansi_colours, uint32_t *ansi_bright_colours,
    uint32_t *default_bg, uint32_t *default_fg,
    uint32_t *default_bg_bright, uint32_t *default_fg_bright,
    void *font, size_t font_width, size_t font_height, size_t font_spacing,
    size_t margin
) {
    if (red_mask_size == 8 && (red_mask_size != green_mask_size || red_mask_size != blue_mask_size)) {
        return NULL;
    }
    
    if (bpp != 32 && bpp != 24 && bpp != 16) {
        return NULL;
    }

    if (_malloc == NULL) {
#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
        if (bump_allocated_instance == true) {
            return NULL;
        }
        _malloc = bump_alloc;
        // Limit terminal size if needed
        if (width > FLANTERM_FB_WIDTH_LIMIT || height > FLANTERM_FB_HEIGHT_LIMIT) {
            size_t width_limit = width > FLANTERM_FB_WIDTH_LIMIT ? FLANTERM_FB_WIDTH_LIMIT : width;
            size_t height_limit = height > FLANTERM_FB_HEIGHT_LIMIT ? FLANTERM_FB_HEIGHT_LIMIT : height;

            framebuffer = (uint32_t *)((uintptr_t)framebuffer + ((((height / 2) - (height_limit / 2)) * pitch) + (((width / 2) - (width_limit / 2)) * (ctx->bpp / 8))));

            width = width_limit;
            height = height_limit;
        }
#else
        return NULL;
#endif
    }

    struct flanterm_fb_context *ctx = NULL;
    ctx = _malloc(sizeof(struct flanterm_fb_context));
    if (ctx == NULL) {
        goto fail;
    }

    struct flanterm_context *_ctx = (void *)ctx;
    memset(ctx, 0, sizeof(struct flanterm_fb_context));

    ctx->bpp = bpp;

    ctx->red_mask_size = red_mask_size;
    ctx->red_mask_shift = red_mask_shift;
    ctx->green_mask_size = green_mask_size;
    ctx->green_mask_shift = green_mask_shift;
    ctx->blue_mask_size = blue_mask_size;
    ctx->blue_mask_shift = blue_mask_shift;

    if (ansi_colours != NULL) {
        for (size_t i = 0; i < 8; i++) {
            ctx->ansi_colours[i] = convert_colour(_ctx, ansi_colours[i]);
        }
    } else {
        ctx->ansi_colours[0] = convert_colour(_ctx, 0x00000000); // black
        ctx->ansi_colours[1] = convert_colour(_ctx, 0x00aa0000); // red
        ctx->ansi_colours[2] = convert_colour(_ctx, 0x0000aa00); // green
        ctx->ansi_colours[3] = convert_colour(_ctx, 0x00aa5500); // brown
        ctx->ansi_colours[4] = convert_colour(_ctx, 0x000000aa); // blue
        ctx->ansi_colours[5] = convert_colour(_ctx, 0x00aa00aa); // magenta
        ctx->ansi_colours[6] = convert_colour(_ctx, 0x0000aaaa); // cyan
        ctx->ansi_colours[7] = convert_colour(_ctx, 0x00aaaaaa); // grey
    }

    if (ansi_bright_colours != NULL) {
        for (size_t i = 0; i < 8; i++) {
            ctx->ansi_bright_colours[i] = convert_colour(_ctx, ansi_bright_colours[i]);
        }
    } else {
        ctx->ansi_bright_colours[0] = convert_colour(_ctx, 0x00555555); // black
        ctx->ansi_bright_colours[1] = convert_colour(_ctx, 0x00ff5555); // red
        ctx->ansi_bright_colours[2] = convert_colour(_ctx, 0x0055ff55); // green
        ctx->ansi_bright_colours[3] = convert_colour(_ctx, 0x00ffff55); // brown
        ctx->ansi_bright_colours[4] = convert_colour(_ctx, 0x005555ff); // blue
        ctx->ansi_bright_colours[5] = convert_colour(_ctx, 0x00ff55ff); // magenta
        ctx->ansi_bright_colours[6] = convert_colour(_ctx, 0x0055ffff); // cyan
        ctx->ansi_bright_colours[7] = convert_colour(_ctx, 0x00ffffff); // grey
    }

    if (default_bg != NULL) {
        ctx->default_bg = convert_colour(_ctx, *default_bg);
    } else {
        ctx->default_bg = 0x00000000; // background (black)
    }

    if (default_fg != NULL) {
        ctx->default_fg = convert_colour(_ctx, *default_fg);
    } else {
        ctx->default_fg = convert_colour(_ctx, 0x00aaaaaa); // foreground (grey)
    }

    if (default_bg_bright != NULL) {
        ctx->default_bg_bright = convert_colour(_ctx, *default_bg_bright);
    } else {
        ctx->default_bg_bright = convert_colour(_ctx, 0x00555555); // background (black)
    }

    if (default_fg_bright != NULL) {
        ctx->default_fg_bright = convert_colour(_ctx, *default_fg_bright);
    } else {
        ctx->default_fg_bright = convert_colour(_ctx, 0x00ffffff); // foreground (grey)
    }

    ctx->text_fg = ctx->default_fg;
    ctx->text_bg = 0xffffffff;

    ctx->framebuffer = (void *)framebuffer;
    ctx->width = width;
    ctx->height = height;
    ctx->pitch = pitch;

#define FONT_BYTES ((font_width * font_height * FLANTERM_FB_FONT_GLYPHS) / 8)

    if (font != NULL) {
        ctx->font_width = font_width;
        ctx->font_height = font_height;
        ctx->font_bits_size = FONT_BYTES;
        ctx->font_bits = _malloc(ctx->font_bits_size);
        if (ctx->font_bits == NULL) {
            goto fail;
        }
        memcpy(ctx->font_bits, font, ctx->font_bits_size);
    } else {
        // removed
        goto fail;
    }

#undef FONT_BYTES

    ctx->font_width += font_spacing;

    ctx->font_bool_size = FLANTERM_FB_FONT_GLYPHS * font_height * ctx->font_width * sizeof(bool);
    ctx->font_bool = _malloc(ctx->font_bool_size);
    if (ctx->font_bool == NULL) {
        goto fail;
    }

    for (size_t i = 0; i < FLANTERM_FB_FONT_GLYPHS; i++) {
        uint8_t *glyph = &ctx->font_bits[i * font_height];

        for (size_t y = 0; y < font_height; y++) {
            // NOTE: the characters in VGA fonts are always one byte wide.
            // 9 dot wide fonts have 8 dots and one empty column, except
            // characters 0xC0-0xDF replicate column 9.
            for (size_t x = 0; x < 8; x++) {
                size_t offset = i * font_height * ctx->font_width + y * ctx->font_width + x;

                if ((glyph[y] & (0x80 >> x))) {
                    ctx->font_bool[offset] = true;
                } else {
                    ctx->font_bool[offset] = false;
                }
            }
            // fill columns above 8 like VGA Line Graphics Mode does
            for (size_t x = 8; x < ctx->font_width; x++) {
                size_t offset = i * font_height * ctx->font_width + y * ctx->font_width + x;

                if (i >= 0xc0 && i <= 0xdf) {
                    ctx->font_bool[offset] = (glyph[y] & 1);
                } else {
                    ctx->font_bool[offset] = false;
                }
            }
        }
    }

    ctx->font_scale_x = 1;
    ctx->font_scale_y = 1;

    ctx->glyph_width = ctx->font_width;
    ctx->glyph_height = font_height;

    _ctx->cols = (ctx->width - margin * 2) / ctx->glyph_width;
    _ctx->rows = (ctx->height - margin * 2) / ctx->glyph_height;

    ctx->offset_x = margin + ((ctx->width - margin * 2) % ctx->glyph_width) / 2;
    ctx->offset_y = margin + ((ctx->height - margin * 2) % ctx->glyph_height) / 2;

    ctx->grid_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_char);
    ctx->grid = _malloc(ctx->grid_size);
    if (ctx->grid == NULL) {
        goto fail;
    }
    for (size_t i = 0; i < _ctx->rows * _ctx->cols; i++) {
        ctx->grid[i].c = ' ';
        ctx->grid[i].fg = ctx->text_fg;
        ctx->grid[i].bg = ctx->text_bg;
    }

    ctx->queue_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_queue_item);
    ctx->queue = _malloc(ctx->queue_size);
    if (ctx->queue == NULL) {
        goto fail;
    }
    ctx->queue_i = 0;
    memset(ctx->queue, 0, ctx->queue_size);

    ctx->map_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_queue_item *);
    ctx->map = _malloc(ctx->map_size);
    if (ctx->map == NULL) {
        goto fail;
    }
    memset(ctx->map, 0, ctx->map_size);

    switch (bpp) {
        case 32:
            ctx->plot_char = plot_char_unscaled_uncanvas_32;
            break;
        case 24:
            ctx->plot_char = plot_char_unscaled_uncanvas_24;
            break;
        case 16:
            ctx->plot_char = plot_char_unscaled_uncanvas_16;
            break;
    }

    _ctx->raw_putchar = flanterm_fb_raw_putchar;
    _ctx->clear = flanterm_fb_clear;
    _ctx->set_cursor_pos = flanterm_fb_set_cursor_pos;
    _ctx->get_cursor_pos = flanterm_fb_get_cursor_pos;
    _ctx->set_text_fg = flanterm_fb_set_text_fg;
    _ctx->set_text_bg = flanterm_fb_set_text_bg;
    _ctx->set_text_fg_bright = flanterm_fb_set_text_fg_bright;
    _ctx->set_text_bg_bright = flanterm_fb_set_text_bg_bright;
    _ctx->set_text_fg_rgb = flanterm_fb_set_text_fg_rgb;
    _ctx->set_text_bg_rgb = flanterm_fb_set_text_bg_rgb;
    _ctx->set_text_fg_default = flanterm_fb_set_text_fg_default;
    _ctx->set_text_bg_default = flanterm_fb_set_text_bg_default;
    _ctx->set_text_fg_default_bright = flanterm_fb_set_text_fg_default_bright;
    _ctx->set_text_bg_default_bright = flanterm_fb_set_text_bg_default_bright;
    _ctx->move_character = flanterm_fb_move_character;
    _ctx->scroll = flanterm_fb_scroll;
    _ctx->revscroll = flanterm_fb_revscroll;
    _ctx->swap_palette = flanterm_fb_swap_palette;
    _ctx->save_state = flanterm_fb_save_state;
    _ctx->restore_state = flanterm_fb_restore_state;
    _ctx->double_buffer_flush = flanterm_fb_double_buffer_flush;
    _ctx->full_refresh = flanterm_fb_full_refresh;
    _ctx->deinit = flanterm_fb_deinit;

    flanterm_context_reinit(_ctx);
    flanterm_fb_full_refresh(_ctx);

#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
    if (_malloc == bump_alloc) {
        bump_allocated_instance = true;
    }
#endif

    return _ctx;

fail:
    if (ctx == NULL) {
        return NULL;
    }

#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
    if (_malloc == bump_alloc) {
        bump_alloc_ptr = 0;
        return NULL;
    }
#endif

    if (_free == NULL) {
        return NULL;
    }

    if (ctx->map != NULL) {
        _free(ctx->map, ctx->map_size);
    }
    if (ctx->queue != NULL) {
        _free(ctx->queue, ctx->queue_size);
    }
    if (ctx->grid != NULL) {
        _free(ctx->grid, ctx->grid_size);
    }
    if (ctx->font_bool != NULL) {
        _free(ctx->font_bool, ctx->font_bool_size);
    }
    if (ctx->font_bits != NULL) {
        _free(ctx->font_bits, ctx->font_bits_size);
    }
    if (ctx != NULL) {
        _free(ctx, sizeof(struct flanterm_fb_context));
    }

    return NULL;
}
