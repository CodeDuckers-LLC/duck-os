#include "desktop/app_registry.h"
#include "desktop/desktop.h"
#include "desktop/files_app.h"
#include "desktop/terminal_app.h"
#include "desktop/text_editor_app.h"
#include "lib/string.h"

#define DESKTOP_REGISTRY_MAX_APPS 8U
#define DESKTOP_REGISTRY_MAX_INSTANCES 8U

static desktop_app_t desktop_registry_apps[DESKTOP_REGISTRY_MAX_APPS];
static unsigned int desktop_registry_app_count;
static desktop_app_instance_t desktop_registry_instances[DESKTOP_REGISTRY_MAX_INSTANCES];
static unsigned int desktop_registry_next_instance_id = 1U;
static int desktop_registry_initialized;

static void desktop_registry_ensure_defaults(void);
static desktop_app_instance_t *desktop_alloc_app_instance(const desktop_app_t *app, const char *argument);

static void desktop_registry_ensure_defaults(void)
{
    desktop_app_t app;

    if (desktop_registry_initialized)
    {
        return;
    }

    desktop_registry_initialized = 1;
    memset(&app, 0, sizeof(app));

    strlcpy(app.name, "terminal", sizeof(app.name));
    strlcpy(app.display_name, "Terminal", sizeof(app.display_name));
    app.on_start = terminal_app_start;
    app.on_event = terminal_app_event;
    app.on_render = terminal_app_render;
    app.on_close = terminal_app_close;
    (void)desktop_register_app(&app);

    memset(&app, 0, sizeof(app));
    strlcpy(app.name, "editor", sizeof(app.name));
    strlcpy(app.display_name, "Editor", sizeof(app.display_name));
    app.on_start = text_editor_app_start;
    app.on_event = text_editor_app_event;
    app.on_render = text_editor_app_render;
    app.on_close = text_editor_app_close;
    (void)desktop_register_app(&app);

    memset(&app, 0, sizeof(app));
    strlcpy(app.name, "files", sizeof(app.name));
    strlcpy(app.display_name, "Files", sizeof(app.display_name));
    app.on_start = files_app_start;
    app.on_event = files_app_event;
    app.on_render = files_app_render;
    app.on_close = files_app_close;
    (void)desktop_register_app(&app);
}

static desktop_app_instance_t *desktop_alloc_app_instance(const desktop_app_t *app, const char *argument)
{
    unsigned int index;

    if (app == 0)
    {
        return 0;
    }

    for (index = 0; index < DESKTOP_REGISTRY_MAX_INSTANCES; index++)
    {
        if ((desktop_registry_instances[index].flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U)
        {
            memset(&desktop_registry_instances[index], 0, sizeof(desktop_registry_instances[index]));
            desktop_registry_instances[index].id = desktop_registry_next_instance_id++;
            desktop_registry_instances[index].flags = DESKTOP_APP_INSTANCE_FLAG_RUNNING;
            desktop_registry_instances[index].app = app;
            if (argument != 0)
            {
                strlcpy(desktop_registry_instances[index].argument,
                        argument,
                        sizeof(desktop_registry_instances[index].argument));
            }
            return &desktop_registry_instances[index];
        }
    }

    return 0;
}

int desktop_register_app(const desktop_app_t *app)
{
    unsigned int index;

    desktop_registry_ensure_defaults();

    if (app == 0 || app->name[0] == '\0' || app->display_name[0] == '\0' || app->on_start == 0)
    {
        return -1;
    }

    for (index = 0; index < desktop_registry_app_count; index++)
    {
        if (strcmp(desktop_registry_apps[index].name, app->name) == 0)
        {
            desktop_registry_apps[index] = *app;
            return 0;
        }
    }

    if (desktop_registry_app_count >= DESKTOP_REGISTRY_MAX_APPS)
    {
        return -1;
    }

    desktop_registry_apps[desktop_registry_app_count++] = *app;
    return 0;
}

const desktop_app_t *desktop_find_app(const char *name)
{
    unsigned int index;

    desktop_registry_ensure_defaults();

    if (name == 0 || *name == '\0')
    {
        return 0;
    }

    for (index = 0; index < desktop_registry_app_count; index++)
    {
        if (strcmp(desktop_registry_apps[index].name, name) == 0)
        {
            return &desktop_registry_apps[index];
        }
    }

    return 0;
}

int desktop_launch_app(const char *name)
{
    return desktop_launch_app_with_argument(name, 0);
}

int desktop_launch_app_with_argument(const char *name, const char *argument)
{
    const desktop_app_t *app;
    desktop_app_instance_t *instance;
    char message[DESKTOP_APP_NAME_MAX + 24U];

    app = desktop_find_app(name);
    if (app == 0)
    {
        memset(message, 0, sizeof(message));
        strlcpy(message, "No app named ", sizeof(message));
        if (name != 0)
        {
            strlcpy(message + strlen(message), name, sizeof(message) - strlen(message));
        }
        (void)desktop_show_alert("Unknown App", message);
        return -1;
    }

    instance = desktop_alloc_app_instance(app, argument);
    if (instance == 0)
    {
        return -1;
    }

    if (app->on_start(instance) != 0 || instance->window_count == 0U)
    {
        desktop_app_instance_stop(instance);
        return -1;
    }

    return 0;
}

const desktop_app_t *desktop_list_apps(unsigned int *count_out)
{
    desktop_registry_ensure_defaults();

    if (count_out != 0)
    {
        *count_out = desktop_registry_app_count;
    }

    return desktop_registry_apps;
}

const desktop_app_instance_t *desktop_list_app_instances(unsigned int *count_out)
{
    desktop_registry_ensure_defaults();

    if (count_out != 0)
    {
        *count_out = DESKTOP_REGISTRY_MAX_INSTANCES;
    }

    return desktop_registry_instances;
}

unsigned int desktop_app_running_count(const char *name)
{
    unsigned int count;
    unsigned int index;

    desktop_registry_ensure_defaults();

    if (name == 0 || *name == '\0')
    {
        return 0U;
    }

    count = 0U;
    for (index = 0; index < DESKTOP_REGISTRY_MAX_INSTANCES; index++)
    {
        if ((desktop_registry_instances[index].flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U ||
            desktop_registry_instances[index].app == 0)
        {
            continue;
        }
        if (strcmp(desktop_registry_instances[index].app->name, name) == 0)
        {
            count++;
        }
    }

    return count;
}

void desktop_app_instance_window_opened(desktop_app_instance_t *instance)
{
    if (instance == 0 || (instance->flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U)
    {
        return;
    }

    instance->window_count++;
}

void desktop_app_instance_window_closed(desktop_app_instance_t *instance)
{
    if (instance == 0 || (instance->flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U)
    {
        return;
    }

    if (instance->window_count > 0U)
    {
        instance->window_count--;
    }
    if (instance->window_count == 0U)
    {
        desktop_app_instance_stop(instance);
    }
}

void desktop_app_instance_stop(desktop_app_instance_t *instance)
{
    if (instance == 0 || (instance->flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U)
    {
        return;
    }

    if (instance->app != 0 && instance->app->on_close != 0)
    {
        instance->app->on_close(instance);
    }

    memset(instance, 0, sizeof(*instance));
}
