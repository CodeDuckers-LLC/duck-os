#include "desktop/desktop_app.h"
#include "lib/string.h"

void desktop_app_init(desktop_app_t *app, unsigned int id, const char *name)
{
    if (app == 0)
    {
        return;
    }

    memset(app, 0, sizeof(*app));
    app->id = id;
    if (name != 0)
    {
        strlcpy(app->name, name, sizeof(app->name));
    }
}

int desktop_app_is_usable(const desktop_app_t *app)
{
    return app != 0 && app->id != 0U;
}
