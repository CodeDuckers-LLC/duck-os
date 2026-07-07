#ifndef DESKTOP_DESKTOP_WINDOW_H
#define DESKTOP_DESKTOP_WINDOW_H

#include "gfx/framebuffer.h"

#define DESKTOP_WINDOW_TITLE_MAX 32U
#define DESKTOP_WINDOW_FLAG_VISIBLE 0x1U

typedef struct desktop_window
{
    unsigned int id;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    unsigned int flags;
    char title[DESKTOP_WINDOW_TITLE_MAX];
} desktop_window_t;

void desktop_window_init(desktop_window_t *window,
                         unsigned int id,
                         int x,
                         int y,
                         unsigned int width,
                         unsigned int height,
                         const char *title);
int desktop_window_is_visible(const desktop_window_t *window);
int desktop_window_content_x(const desktop_window_t *window);
int desktop_window_content_y(const desktop_window_t *window);
unsigned int desktop_window_content_width(const desktop_window_t *window);
unsigned int desktop_window_content_height(const desktop_window_t *window);
unsigned int desktop_window_title_height(void);
int desktop_window_contains_point(const desktop_window_t *window, unsigned int x, unsigned int y);
int desktop_window_title_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y);
void desktop_window_clamp_to_screen(desktop_window_t *window, unsigned int screen_width, unsigned int screen_height);
void desktop_window_draw(const desktop_window_t *window, framebuffer_t *fb, int active);

#endif
