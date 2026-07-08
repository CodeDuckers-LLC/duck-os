#ifndef DESKTOP_DESKTOP_WINDOW_H
#define DESKTOP_DESKTOP_WINDOW_H

#include "gfx/framebuffer.h"

#define DESKTOP_WINDOW_TITLE_MAX 32U
#define DESKTOP_WINDOW_FLAG_VISIBLE 0x1U
#define DESKTOP_WINDOW_FLAG_MINIMIZED 0x2U
#define DESKTOP_WINDOW_FLAG_MAXIMIZED 0x4U
#define DESKTOP_WINDOW_FLAG_NO_CONTROLS 0x8U

#define DESKTOP_WINDOW_CONTROL_NONE 0U
#define DESKTOP_WINDOW_CONTROL_CLOSE 1U
#define DESKTOP_WINDOW_CONTROL_MINIMIZE 2U
#define DESKTOP_WINDOW_CONTROL_MAXIMIZE 3U

struct desktop_event;
struct desktop_app_instance;
struct desktop_window;

typedef void (*desktop_window_draw_fn_t)(const struct desktop_window *window,
                                         framebuffer_t *fb,
                                         void *user_data);
typedef void (*desktop_window_event_fn_t)(const struct desktop_window *window,
                                          const struct desktop_event *event,
                                          void *user_data);

typedef struct desktop_window
{
    unsigned int id;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    unsigned int flags;
    int restore_x;
    int restore_y;
    unsigned int restore_width;
    unsigned int restore_height;
    char title[DESKTOP_WINDOW_TITLE_MAX];
    struct desktop_app_instance *owner;
    desktop_window_draw_fn_t draw;
    desktop_window_event_fn_t handle_event;
    void *user_data;
} desktop_window_t;

void desktop_window_init(desktop_window_t *window,
                         unsigned int id,
                         int x,
                         int y,
                         unsigned int width,
                         unsigned int height,
                         const char *title);
void desktop_window_bind(desktop_window_t *window,
                         desktop_window_draw_fn_t draw,
                         desktop_window_event_fn_t handle_event,
                         void *user_data);
int desktop_window_is_visible(const desktop_window_t *window);
int desktop_window_content_x(const desktop_window_t *window);
int desktop_window_content_y(const desktop_window_t *window);
unsigned int desktop_window_content_width(const desktop_window_t *window);
unsigned int desktop_window_content_height(const desktop_window_t *window);
unsigned int desktop_window_title_height(void);
int desktop_window_contains_point(const desktop_window_t *window, unsigned int x, unsigned int y);
int desktop_window_title_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y);
unsigned int desktop_window_control_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y);
void desktop_window_clamp_to_screen(desktop_window_t *window, unsigned int screen_width, unsigned int screen_height);
void desktop_window_draw(const desktop_window_t *window, framebuffer_t *fb, int active);

#endif
