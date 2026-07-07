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
int gui_window_content_x(const window_t *window);
int gui_window_content_y(const window_t *window);
unsigned int gui_window_content_width(const window_t *window);
unsigned int gui_window_content_height(const window_t *window);
void gui_draw_panel(framebuffer_t *fb,
                    int x,
                    int y,
                    unsigned int width,
                    unsigned int height,
                    unsigned int bg_color,
                    unsigned int border_color);
void gui_draw_label(framebuffer_t *fb,
                    int x,
                    int y,
                    const char *text,
                    unsigned int fg_color,
                    unsigned int bg_color);
void gui_draw_button(framebuffer_t *fb,
                     int x,
                     int y,
                     unsigned int width,
                     unsigned int height,
                     const char *text,
                     int pressed);

#endif
