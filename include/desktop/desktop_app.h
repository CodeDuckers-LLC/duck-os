#ifndef DESKTOP_DESKTOP_APP_H
#define DESKTOP_DESKTOP_APP_H

#define DESKTOP_APP_NAME_MAX 24U

typedef struct desktop_app
{
    unsigned int id;
    char name[DESKTOP_APP_NAME_MAX];
} desktop_app_t;

void desktop_app_init(desktop_app_t *app, unsigned int id, const char *name);
int desktop_app_is_usable(const desktop_app_t *app);

#endif
