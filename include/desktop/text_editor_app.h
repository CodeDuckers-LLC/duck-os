#ifndef DESKTOP_TEXT_EDITOR_APP_H
#define DESKTOP_TEXT_EDITOR_APP_H

#include "desktop/app_registry.h"

int text_editor_app_start(desktop_app_instance_t *instance);
void text_editor_app_event(desktop_app_instance_t *instance,
                           const struct desktop_window *window,
                           const struct desktop_event *event);
void text_editor_app_render(desktop_app_instance_t *instance,
                            const struct desktop_window *window,
                            framebuffer_t *fb);
void text_editor_app_close(desktop_app_instance_t *instance);
int text_editor_app_launch_path(const char *path);

#endif
