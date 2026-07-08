#ifndef DESKTOP_DESKTOP_THEME_H
#define DESKTOP_DESKTOP_THEME_H

typedef struct desktop_theme
{
    unsigned int background;
    unsigned int taskbar;
    unsigned int window_body;
    unsigned int titlebar_active;
    unsigned int titlebar_inactive;
    unsigned int text;
    unsigned int accent;
} desktop_theme_t;

void desktop_theme_init(void);
const desktop_theme_t *desktop_theme_get(void);
unsigned int desktop_theme_lighten(unsigned int color, unsigned int amount);
unsigned int desktop_theme_darken(unsigned int color, unsigned int amount);

#endif
