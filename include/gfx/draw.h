#ifndef GFX_DRAW_H
#define GFX_DRAW_H

#include "gfx/framebuffer.h"

void draw_pixel(framebuffer_t *fb, int x, int y, unsigned int color);
void draw_line(framebuffer_t *fb, int x0, int y0, int x1, int y1, unsigned int color);
void draw_rect(framebuffer_t *fb, int x, int y, int width, int height, unsigned int color);
void draw_fill_rect(framebuffer_t *fb, int x, int y, int width, int height, unsigned int color);
void draw_hline(framebuffer_t *fb, int x, int y, int width, unsigned int color);
void draw_vline(framebuffer_t *fb, int x, int y, int height, unsigned int color);

#endif
