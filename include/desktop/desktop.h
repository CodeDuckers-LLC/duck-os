#ifndef DESKTOP_DESKTOP_H
#define DESKTOP_DESKTOP_H

int desktop_init(void);
int desktop_enter(void);
void desktop_exit(void);
void desktop_run(void);
void desktop_run_once(void);
void desktop_render(void);
int desktop_is_active(void);

#endif
