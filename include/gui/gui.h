#ifndef GUI_GUI_H
#define GUI_GUI_H

#include "gfx/framebuffer.h"

#define GUI_TITLE_MAX 32U

typedef struct window window_t;
typedef void (*window_draw_fn_t)(window_t *window, framebuffer_t *fb);

struct window
{
    unsigned int id;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    char title[GUI_TITLE_MAX];
    int visible;
    window_draw_fn_t draw;
};

window_t *gui_create_window(int x,
                            int y,
                            unsigned int width,
                            unsigned int height,
                            const char *title,
                            window_draw_fn_t draw);
void gui_destroy_window(unsigned int id);
void gui_draw_all(void);
void gui_attach_framebuffer(framebuffer_t *fb);
framebuffer_t *gui_framebuffer(void);

#endif
