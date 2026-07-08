#ifndef DESKTOP_TERMINAL_APP_H
#define DESKTOP_TERMINAL_APP_H

#include "desktop/app_registry.h"

int terminal_app_start(desktop_app_instance_t *instance);
void terminal_app_event(desktop_app_instance_t *instance,
                        const struct desktop_window *window,
                        const struct desktop_event *event);
void terminal_app_render(desktop_app_instance_t *instance,
                         const struct desktop_window *window,
                         framebuffer_t *fb);
void terminal_app_close(desktop_app_instance_t *instance);

#endif
