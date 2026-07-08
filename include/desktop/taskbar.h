#ifndef DESKTOP_TASKBAR_H
#define DESKTOP_TASKBAR_H

#include "desktop/app_registry.h"
#include "desktop/desktop_window.h"
#include "gfx/framebuffer.h"

unsigned int desktop_taskbar_height(void);
void desktop_taskbar_draw(framebuffer_t *fb,
                          const desktop_window_t *windows,
                          unsigned int window_count,
                          unsigned int focused_window_id,
                          const desktop_app_t *apps,
                          unsigned int app_count,
                          unsigned int launcher_selected_index,
                          unsigned long uptime_ms,
                          int launcher_open);
int desktop_taskbar_contains_point(unsigned int screen_height, unsigned int x, unsigned int y);
int desktop_taskbar_launcher_hit_test(unsigned int screen_height, unsigned int x, unsigned int y);
unsigned int desktop_taskbar_launcher_item_hit_test(unsigned int screen_height,
                                                    const desktop_app_t *apps,
                                                    unsigned int app_count,
                                                    unsigned int x,
                                                    unsigned int y);
unsigned int desktop_taskbar_window_button_hit_test(const desktop_window_t *windows,
                                                    unsigned int window_count,
                                                    unsigned int screen_width,
                                                    unsigned int screen_height,
                                                    unsigned int x,
                                                    unsigned int y);
unsigned int desktop_taskbar_visible_window_count(const desktop_window_t *windows, unsigned int window_count);

#endif
