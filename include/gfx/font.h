#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "gfx/framebuffer.h"

#define GFX_FONT_WIDTH 8
#define GFX_FONT_HEIGHT 8
#define GFX_FONT_FIRST_CHAR 32
#define GFX_FONT_LAST_CHAR 126

const unsigned char *gfx_font8x8_get(char ch);
void gfx_draw_char(framebuffer_t *fb, int x, int y, char ch, unsigned int fg_color, unsigned int bg_color);
void gfx_draw_string(framebuffer_t *fb, int x, int y, const char *text, unsigned int fg_color, unsigned int bg_color);

#endif
