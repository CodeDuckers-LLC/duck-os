#ifndef DESKTOP_DESKTOP_H
#define DESKTOP_DESKTOP_H

#include "desktop/desktop_event.h"
#include "desktop/app_registry.h"
#include "desktop/desktop_window.h"

int desktop_init(void);
int desktop_enter(void);
void desktop_exit(void);
void desktop_run(void);
void desktop_run_once(void);
void desktop_render(void);
int desktop_is_active(void);
void desktop_focus_window(desktop_window_t *window);
void desktop_bring_to_front(desktop_window_t *window);
unsigned int desktop_work_area_height(void);
int desktop_open_app_window(const char *title);
int desktop_open_custom_window(const char *title,
                               unsigned int width,
                               unsigned int height,
                               desktop_window_draw_fn_t draw,
                               desktop_window_event_fn_t handle_event,
                               void *user_data);
int desktop_open_app_instance_window(desktop_app_instance_t *instance,
                                     const char *title,
                                     unsigned int width,
                                     unsigned int height,
                                     void *user_data);
int desktop_open_file(const char *path);
void desktop_destroy_window(desktop_window_t *window);
void desktop_minimize_window(desktop_window_t *window);
void desktop_toggle_maximize_window(desktop_window_t *window);

#endif
