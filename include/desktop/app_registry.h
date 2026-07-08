#ifndef DESKTOP_APP_REGISTRY_H
#define DESKTOP_APP_REGISTRY_H

#include "gfx/framebuffer.h"

#define DESKTOP_APP_NAME_MAX 24U
#define DESKTOP_APP_DISPLAY_NAME_MAX 32U
#define DESKTOP_APP_ARGUMENT_MAX 64U
#define DESKTOP_APP_INSTANCE_FLAG_RUNNING 0x1U

struct desktop_app_instance;
struct desktop_event;
struct desktop_window;

typedef int (*desktop_app_start_fn_t)(struct desktop_app_instance *instance);
typedef void (*desktop_app_event_fn_t)(struct desktop_app_instance *instance,
                                       const struct desktop_window *window,
                                       const struct desktop_event *event);
typedef void (*desktop_app_render_fn_t)(struct desktop_app_instance *instance,
                                        const struct desktop_window *window,
                                        framebuffer_t *fb);
typedef void (*desktop_app_close_fn_t)(struct desktop_app_instance *instance);

typedef struct desktop_app
{
    char name[DESKTOP_APP_NAME_MAX];
    char display_name[DESKTOP_APP_DISPLAY_NAME_MAX];
    desktop_app_start_fn_t on_start;
    desktop_app_event_fn_t on_event;
    desktop_app_render_fn_t on_render;
    desktop_app_close_fn_t on_close;
} desktop_app_t;

typedef struct desktop_app_instance
{
    unsigned int id;
    unsigned int flags;
    unsigned int window_count;
    const desktop_app_t *app;
    void *user_data;
    char argument[DESKTOP_APP_ARGUMENT_MAX];
} desktop_app_instance_t;

int desktop_register_app(const desktop_app_t *app);
const desktop_app_t *desktop_find_app(const char *name);
int desktop_launch_app(const char *name);
int desktop_launch_app_with_argument(const char *name, const char *argument);
const desktop_app_t *desktop_list_apps(unsigned int *count_out);
const desktop_app_instance_t *desktop_list_app_instances(unsigned int *count_out);
unsigned int desktop_app_running_count(const char *name);
void desktop_app_instance_window_opened(desktop_app_instance_t *instance);
void desktop_app_instance_window_closed(desktop_app_instance_t *instance);
void desktop_app_instance_stop(desktop_app_instance_t *instance);

#endif
