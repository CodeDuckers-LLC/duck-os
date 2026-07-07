#ifndef GFX_CURSOR_H
#define GFX_CURSOR_H

#include "gfx/framebuffer.h"

void gfx_cursor_attach(framebuffer_t *fb);
int gfx_cursor_available(void);
void gfx_cursor_move(unsigned int x, unsigned int y);
void gfx_cursor_refresh(void);
unsigned int gfx_cursor_x(void);
unsigned int gfx_cursor_y(void);

#endif
