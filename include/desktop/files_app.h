#ifndef DESKTOP_FILES_APP_H
#define DESKTOP_FILES_APP_H

#include "desktop/app_registry.h"

int files_app_start(desktop_app_instance_t *instance);
void files_app_event(desktop_app_instance_t *instance,
                     const struct desktop_window *window,
                     const struct desktop_event *event);
void files_app_render(desktop_app_instance_t *instance,
                      const struct desktop_window *window,
                      framebuffer_t *fb);
void files_app_close(desktop_app_instance_t *instance);

#endif
