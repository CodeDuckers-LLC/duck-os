#ifndef DESKTOP_DESKTOP_H
#define DESKTOP_DESKTOP_H

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

#endif
