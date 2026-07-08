#include "desktop/desktop.h"
#include "desktop/app_registry.h"
#include "desktop/desktop_event.h"
#include "desktop/desktop_input.h"
#include "desktop/desktop_window.h"
#include "desktop/taskbar.h"
#include "arch/aarch64/cpu.h"
#include "drivers/virtio_gpu.h"
#include "fs/vfs.h"
#include "gfx/cursor.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "kernel/console.h"
#include "kernel/input.h"
#include "kernel/timer.h"
#include "lib/string.h"

#define DESKTOP_MAX_WINDOWS 8U
#define DESKTOP_BG_COLOR 0xff6d8fa4U
#define DESKTOP_ACCENT_COLOR 0xff89aebfU
#define DESKTOP_PANEL_COLOR 0xff7ea2b3U
#define DESKTOP_TEXT_COLOR 0xff102030U
#define DESKTOP_DIALOG_MESSAGE_MAX 128U

typedef struct desktop_dialog_state
{
    char message[DESKTOP_DIALOG_MESSAGE_MAX];
} desktop_dialog_state_t;

typedef struct desktop_state
{
    framebuffer_t *fb;
    desktop_window_t windows[DESKTOP_MAX_WINDOWS];
    unsigned int window_count;
    unsigned int frame_counter;
    unsigned int cursor_x;
    unsigned int cursor_y;
    unsigned int focused_window_index;
    unsigned int primary_button_down;
    unsigned int drag_window_id;
    unsigned int drag_offset_x;
    unsigned int drag_offset_y;
    unsigned int saved_console_mode;
    unsigned int saved_input_mode;
    unsigned int launcher_selected_index;
    int launcher_open;
    int initialized;
    int active;
    int dirty;
} desktop_state_t;

static desktop_state_t desktop_state;
static desktop_dialog_state_t desktop_open_error_dialog;

static void desktop_create_layout(void);
static void desktop_handle_event(const desktop_event_t *event);
static void desktop_draw_background(framebuffer_t *fb);
static void desktop_draw_window_contents(framebuffer_t *fb, const desktop_window_t *window);
static void desktop_dialog_draw(const desktop_window_t *window, framebuffer_t *fb, void *user_data);
static int desktop_display_available(void);
static unsigned int desktop_find_window_index_by_id(unsigned int id);
static desktop_window_t *desktop_find_window_by_id(unsigned int id);
static void desktop_cycle_focus_by_id(unsigned int window_id);
static unsigned int desktop_next_window_id(void);
static void desktop_toggle_launcher(void);
static void desktop_launcher_move_selection(int delta);
static void desktop_launcher_launch_selected(void);
static void desktop_notify_window_close(desktop_window_t *window);
static void desktop_dispatch_window_event(desktop_window_t *window, const desktop_event_t *event);
static const char *desktop_path_extension(const char *path);
static int desktop_extension_matches(const char *extension, const char *expected);
static void desktop_set_dialog_message(desktop_dialog_state_t *state, const char *prefix, const char *path);
static int desktop_show_open_error(const char *prefix, const char *path);
static void desktop_recompute_focus(void);

static void desktop_create_layout(void)
{
    desktop_state.window_count = 0U;

    desktop_window_init(&desktop_state.windows[desktop_state.window_count++],
                        1U,
                        40,
                        32,
                        268U,
                        132U,
                        "Welcome");
    desktop_window_init(&desktop_state.windows[desktop_state.window_count++],
                        2U,
                        336,
                        72,
                        232U,
                        116U,
                        "System");
}

static void desktop_handle_event(const desktop_event_t *event)
{
    desktop_window_t *window;

    if (event == 0)
    {
        return;
    }

    switch (event->type)
    {
    case DESKTOP_EVENT_REDRAW:
        desktop_state.dirty = 1;
        break;
    case DESKTOP_EVENT_KEY:
        if (event->input.data.keycode == INPUT_KEY_F1 &&
            (event->input.pressed == INPUT_KEY_PRESS || event->input.pressed == INPUT_KEY_REPEAT))
        {
            desktop_toggle_launcher();
            break;
        }
        if (desktop_state.launcher_open &&
            (event->input.pressed == INPUT_KEY_PRESS || event->input.pressed == INPUT_KEY_REPEAT))
        {
            if (event->input.data.keycode == INPUT_KEY_UP)
            {
                desktop_launcher_move_selection(-1);
                break;
            }
            if (event->input.data.keycode == INPUT_KEY_DOWN)
            {
                desktop_launcher_move_selection(1);
                break;
            }
            if (event->input.data.keycode == INPUT_KEY_ENTER)
            {
                desktop_launcher_launch_selected();
                break;
            }
            if (event->input.data.keycode == INPUT_KEY_ESC)
            {
                desktop_state.launcher_open = 0;
                desktop_state.dirty = 1;
                break;
            }
        }
        if (event->input.data.keycode == INPUT_KEY_TAB &&
            (event->input.pressed == INPUT_KEY_PRESS || event->input.pressed == INPUT_KEY_REPEAT))
        {
            desktop_cycle_focus_by_id(event->target_window_id);
        }
        window = desktop_find_window_by_id(event->target_window_id);
        desktop_dispatch_window_event(window, event);
        desktop_state.dirty = 1;
        break;
    case DESKTOP_EVENT_CHAR:
        if (event->character == '\t')
        {
            desktop_cycle_focus_by_id(event->target_window_id);
        }
        window = desktop_find_window_by_id(event->target_window_id);
        desktop_dispatch_window_event(window, event);
        desktop_state.dirty = 1;
        break;
    case DESKTOP_EVENT_BUTTON_DOWN:
    {
        const desktop_app_t *apps;
        unsigned int app_count;
        unsigned int button_window_id;
        unsigned int launcher_index;
        unsigned int window_index;

        apps = desktop_list_apps(&app_count);

        if (desktop_taskbar_launcher_hit_test(desktop_state.fb->height, event->cursor_x, event->cursor_y))
        {
            desktop_toggle_launcher();
            desktop_state.drag_window_id = 0U;
            break;
        }

        launcher_index = desktop_taskbar_launcher_item_hit_test(desktop_state.fb->height,
                                                                apps,
                                                                app_count,
                                                                event->cursor_x,
                                                                event->cursor_y);
        if (desktop_state.launcher_open && launcher_index < app_count)
        {
            (void)desktop_launch_app(apps[launcher_index].name);
            desktop_state.launcher_open = 0;
            desktop_state.launcher_selected_index = launcher_index;
            desktop_state.drag_window_id = 0U;
            desktop_state.dirty = 1;
            break;
        }

        button_window_id = desktop_taskbar_window_button_hit_test(desktop_state.windows,
                                                                  desktop_state.window_count,
                                                                  desktop_state.fb->width,
                                                                  desktop_state.fb->height,
                                                                  event->cursor_x,
                                                                  event->cursor_y);
        if (button_window_id != 0U)
        {
            window = desktop_find_window_by_id(button_window_id);
            if (window != 0)
            {
                if ((window->flags & DESKTOP_WINDOW_FLAG_MINIMIZED) != 0U)
                {
                    window->flags &= ~DESKTOP_WINDOW_FLAG_MINIMIZED;
                    window->flags |= DESKTOP_WINDOW_FLAG_VISIBLE;
                }
                desktop_focus_window(window);
                desktop_bring_to_front(window);
            }
            desktop_state.launcher_open = 0;
            desktop_state.drag_window_id = 0U;
            desktop_state.dirty = 1;
            break;
        }

        window_index = desktop_find_window_index_by_id(event->target_window_id);
        if (window_index < desktop_state.window_count)
        {
            window = &desktop_state.windows[window_index];
            desktop_focus_window(window);
            desktop_bring_to_front(window);
            window = &desktop_state.windows[desktop_state.focused_window_index];
            desktop_state.launcher_open = 0;

            switch (desktop_window_control_hit_test(window, event->cursor_x, event->cursor_y))
            {
            case DESKTOP_WINDOW_CONTROL_CLOSE:
                desktop_destroy_window(window);
                break;
            case DESKTOP_WINDOW_CONTROL_MINIMIZE:
                desktop_minimize_window(window);
                break;
            case DESKTOP_WINDOW_CONTROL_MAXIMIZE:
                desktop_toggle_maximize_window(window);
                break;
            default:
                if (desktop_window_title_hit_test(window, event->cursor_x, event->cursor_y))
                {
                    desktop_state.drag_window_id = window->id;
                    desktop_state.drag_offset_x = event->cursor_x - (unsigned int)window->x;
                    desktop_state.drag_offset_y = event->cursor_y - (unsigned int)window->y;
                }
                break;
            }
        }
        else if (desktop_taskbar_contains_point(desktop_state.fb->height, event->cursor_x, event->cursor_y))
        {
            desktop_state.launcher_open = 0;
        }
        else
        {
            desktop_state.launcher_open = 0;
            desktop_state.drag_window_id = 0U;
        }
        desktop_state.dirty = 1;
        window = desktop_find_window_by_id(event->target_window_id);
        desktop_dispatch_window_event(window, event);
        break;
    }
    case DESKTOP_EVENT_BUTTON_UP:
        desktop_state.primary_button_down = 0U;
        desktop_state.drag_window_id = 0U;
        desktop_state.dirty = 1;
        window = desktop_find_window_by_id(event->target_window_id);
        desktop_dispatch_window_event(window, event);
        break;
    case DESKTOP_EVENT_CURSOR_MOVE:
        window = desktop_find_window_by_id(desktop_state.drag_window_id);
        if (window != 0)
        {
            window->x = (int)event->cursor_x - (int)desktop_state.drag_offset_x;
            window->y = (int)event->cursor_y - (int)desktop_state.drag_offset_y;
            desktop_window_clamp_to_screen(window, desktop_state.fb->width, desktop_work_area_height());
        }
        desktop_state.dirty = 1;
        break;
    default:
        break;
    }
}

static void desktop_draw_background(framebuffer_t *fb)
{
    unsigned int right_panel_width;

    fb_clear(fb, DESKTOP_BG_COLOR);
    draw_fill_rect(fb, 0, 0, (int)fb->width, 52, DESKTOP_ACCENT_COLOR);

    right_panel_width = fb->width / 4U;
    if (right_panel_width > 0U)
    {
        draw_fill_rect(fb,
                       (int)(fb->width - right_panel_width),
                       52,
                       (int)right_panel_width,
                       (int)desktop_work_area_height() - 52,
                       DESKTOP_PANEL_COLOR);
    }

    gfx_draw_string(fb, 24, 18, "duck-os desktop", 0xffffffffU, DESKTOP_ACCENT_COLOR);
    gfx_draw_string(fb,
                    (int)fb->width - 176,
                    18,
                    "F1 launcher  Enter click",
                    0xffffffffU,
                    DESKTOP_ACCENT_COLOR);
}

static void desktop_draw_window_contents(framebuffer_t *fb, const desktop_window_t *window)
{
    int x;
    int y;
    unsigned int width;
    unsigned int height;

    x = desktop_window_content_x(window);
    y = desktop_window_content_y(window);
    width = desktop_window_content_width(window);
    height = desktop_window_content_height(window);
    if (width == 0U || height == 0U)
    {
        return;
    }

    draw_fill_rect(fb, x, y, (int)width, (int)height, 0xffeef4f7U);
    draw_rect(fb, x, y, (int)width, (int)height, 0xff8ca2adU);

    if (window->owner != 0 && window->owner->app != 0 && window->owner->app->on_render != 0)
    {
        window->owner->app->on_render(window->owner, window, fb);
        return;
    }

    if (window->draw != 0)
    {
        window->draw(window, fb, window->user_data);
        return;
    }

    if (window->id == 1U)
    {
        gfx_draw_string(fb, x + 8, y + 10, "Desktop runtime online", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 26, "Title bars can be dragged", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 42, "Taskbar launcher below", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
    }
    else if (window->id == 2U)
    {
        gfx_draw_string(fb, x + 8, y + 10, "Renderer: framebuffer", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 26, "Input: focus + drag routing", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 42, "Loop: cooperative", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        if (desktop_state.primary_button_down != 0U)
        {
            gfx_draw_string(fb, x + 8, y + 58, "Pointer: button down", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        }
        else
        {
            gfx_draw_string(fb, x + 8, y + 58, "Pointer: button up", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        }
    }
    else
    {
        gfx_draw_string(fb, x + 8, y + 10, "App placeholder window", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 26, window->title, DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 42, "Kernel-mode built-in app", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 58, "Registry launch path works", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
    }
}

static void desktop_dialog_draw(const desktop_window_t *window, framebuffer_t *fb, void *user_data)
{
    desktop_dialog_state_t *state;
    int x;
    int y;
    unsigned int width;
    unsigned int height;

    state = (desktop_dialog_state_t *)user_data;
    if (window == 0 || state == 0)
    {
        return;
    }

    x = desktop_window_content_x(window);
    y = desktop_window_content_y(window);
    width = desktop_window_content_width(window);
    height = desktop_window_content_height(window);

    draw_fill_rect(fb, x, y, (int)width, (int)height, 0xfffff2dbU);
    draw_rect(fb, x, y, (int)width, (int)height, 0xff9d7f67U);
    gfx_draw_string(fb, x + 8, y + 10, state->message, 0xff3b2f2fU, 0xfffff2dbU);
    gfx_draw_string(fb, x + 8, y + 28, "Use editor for .txt .md .log", 0xff6a5646U, 0xfffff2dbU);
}

int desktop_init(void)
{
    framebuffer_t *fb;

    fb = console_graphics_framebuffer();
    if (fb == 0 && virtio_gpu_available())
    {
        fb = (framebuffer_t *)virtio_gpu_framebuffer();
    }

    if (fb == 0 || fb->buffer == 0)
    {
        return -1;
    }

    memset(&desktop_state, 0, sizeof(desktop_state));
    desktop_state.fb = fb;
    desktop_state.initialized = 1;
    desktop_state.active = 0;
    desktop_state.dirty = 1;
    (void)desktop_list_apps(0);
    desktop_create_layout();
    desktop_input_reset(fb->width,
                        fb->height,
                        &desktop_state.cursor_x,
                        &desktop_state.cursor_y,
                        &desktop_state.focused_window_index,
                        &desktop_state.primary_button_down);
    return 0;
}

int desktop_enter(void)
{
    if (!desktop_state.initialized && desktop_init() != 0)
    {
        return -1;
    }

    desktop_state.saved_console_mode = console_output_mode();
    desktop_state.saved_input_mode = console_input_mode();
    desktop_state.active = 1;
    desktop_state.dirty = 1;
    desktop_state.drag_window_id = 0U;
    desktop_input_reset(desktop_state.fb->width,
                        desktop_state.fb->height,
                        &desktop_state.cursor_x,
                        &desktop_state.cursor_y,
                        &desktop_state.focused_window_index,
                        &desktop_state.primary_button_down);

    console_set_output_mode(CONSOLE_SINK_SERIAL);
    if (input_keyboard_available())
    {
        console_set_input_mode(INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD);
    }
    else
    {
        console_set_input_mode(INPUT_SOURCE_SERIAL);
    }

    if (!gfx_cursor_available())
    {
        gfx_cursor_attach(desktop_state.fb);
    }
    gfx_cursor_move(desktop_state.cursor_x, desktop_state.cursor_y);

    return 0;
}

void desktop_exit(void)
{
    if (!desktop_state.initialized)
    {
        return;
    }

    desktop_state.active = 0;
    console_set_output_mode(desktop_state.saved_console_mode);
    console_set_input_mode(desktop_state.saved_input_mode);
}

void desktop_focus_window(desktop_window_t *window)
{
    unsigned int window_index;

    if (window == 0)
    {
        return;
    }

    window_index = desktop_find_window_index_by_id(window->id);
    if (window_index >= desktop_state.window_count)
    {
        return;
    }
    if ((desktop_state.windows[window_index].flags & DESKTOP_WINDOW_FLAG_MINIMIZED) != 0U)
    {
        desktop_state.windows[window_index].flags &= ~DESKTOP_WINDOW_FLAG_MINIMIZED;
        desktop_state.windows[window_index].flags |= DESKTOP_WINDOW_FLAG_VISIBLE;
    }

    desktop_state.focused_window_index = window_index;
    desktop_state.dirty = 1;
}

void desktop_bring_to_front(desktop_window_t *window)
{
    desktop_window_t moved_window;
    unsigned int window_index;
    unsigned int index;

    if (window == 0)
    {
        return;
    }

    window_index = desktop_find_window_index_by_id(window->id);
    if (window_index >= desktop_state.window_count)
    {
        return;
    }
    if ((desktop_state.windows[window_index].flags & DESKTOP_WINDOW_FLAG_MINIMIZED) != 0U)
    {
        desktop_state.windows[window_index].flags &= ~DESKTOP_WINDOW_FLAG_MINIMIZED;
        desktop_state.windows[window_index].flags |= DESKTOP_WINDOW_FLAG_VISIBLE;
    }

    if (window_index + 1U == desktop_state.window_count)
    {
        desktop_focus_window(window);
        return;
    }

    moved_window = desktop_state.windows[window_index];
    for (index = window_index; index + 1U < desktop_state.window_count; index++)
    {
        desktop_state.windows[index] = desktop_state.windows[index + 1U];
    }
    desktop_state.windows[desktop_state.window_count - 1U] = moved_window;
    desktop_state.focused_window_index = desktop_state.window_count - 1U;
    desktop_state.dirty = 1;
}

void desktop_run_once(void)
{
    desktop_event_t event;

    if (!desktop_state.initialized || !desktop_state.active)
    {
        return;
    }

    desktop_event_init(&event);
    event.type = DESKTOP_EVENT_REDRAW;
    desktop_handle_event(&event);

    if (desktop_state.dirty)
    {
        desktop_render();
    }
}

void desktop_render(void)
{
    const desktop_app_t *apps;
    unsigned int app_count;
    unsigned int index;

    if (!desktop_state.initialized || desktop_state.fb == 0 || desktop_state.fb->buffer == 0)
    {
        return;
    }

    apps = desktop_list_apps(&app_count);
    desktop_draw_background(desktop_state.fb);

    for (index = 0; index < desktop_state.window_count; index++)
    {
        desktop_window_draw(&desktop_state.windows[index],
                            desktop_state.fb,
                            index == desktop_state.focused_window_index);
        if (desktop_window_is_visible(&desktop_state.windows[index]))
        {
            desktop_draw_window_contents(desktop_state.fb, &desktop_state.windows[index]);
        }
    }

    desktop_taskbar_draw(desktop_state.fb,
                         desktop_state.windows,
                         desktop_state.window_count,
                         (desktop_state.focused_window_index < desktop_state.window_count)
                             ? desktop_state.windows[desktop_state.focused_window_index].id
                             : 0U,
                         apps,
                         app_count,
                         desktop_state.launcher_selected_index,
                         timer_uptime_ms(),
                         desktop_state.launcher_open);
    if (gfx_cursor_available())
    {
        gfx_cursor_refresh();
    }
    desktop_state.frame_counter++;
    desktop_state.dirty = 0;
}

void desktop_run(void)
{
    input_event_t input_event;
    desktop_event_t event;
    int handled_event;

    if (!desktop_state.initialized || !desktop_state.active)
    {
        return;
    }

    while (desktop_state.active)
    {
        unsigned int previous_cursor_x;
        unsigned int previous_cursor_y;

        handled_event = 0;
        previous_cursor_x = desktop_state.cursor_x;
        previous_cursor_y = desktop_state.cursor_y;
        input_poll();

        if (gfx_cursor_available())
        {
            desktop_state.cursor_x = gfx_cursor_x();
            desktop_state.cursor_y = gfx_cursor_y();
            if (desktop_state.cursor_x != previous_cursor_x || desktop_state.cursor_y != previous_cursor_y)
            {
                desktop_event_init(&event);
                event.type = DESKTOP_EVENT_CURSOR_MOVE;
                event.target_window_id = (desktop_state.focused_window_index < desktop_state.window_count)
                                             ? desktop_state.windows[desktop_state.focused_window_index].id
                                             : 0U;
                event.cursor_x = desktop_state.cursor_x;
                event.cursor_y = desktop_state.cursor_y;
                desktop_handle_event(&event);
                handled_event = 1;
            }
        }

        while (input_pop_event(&input_event))
        {
            unsigned int route_result;

            route_result = desktop_input_route(&input_event,
                                               desktop_state.windows,
                                               desktop_state.window_count,
                                               desktop_state.fb->width,
                                               desktop_state.fb->height,
                                               &desktop_state.cursor_x,
                                               &desktop_state.cursor_y,
                                               &desktop_state.focused_window_index,
                                               &desktop_state.primary_button_down,
                                               &event);
            if ((route_result & DESKTOP_INPUT_RESULT_EXIT) != 0U)
            {
                desktop_exit();
                break;
            }

            if (route_result != DESKTOP_INPUT_RESULT_NONE)
            {
                desktop_handle_event(&event);
                handled_event = 1;
            }
        }

        if (!desktop_state.active)
        {
            break;
        }

        if (desktop_state.dirty)
        {
            desktop_render();
            if (desktop_display_available())
            {
                (void)virtio_gpu_flush();
            }
        }

        if (!handled_event && !desktop_state.dirty)
        {
            cpu_wait_for_interrupt();
        }

        if (gfx_cursor_available() &&
            (gfx_cursor_x() != desktop_state.cursor_x || gfx_cursor_y() != desktop_state.cursor_y))
        {
            gfx_cursor_move(desktop_state.cursor_x, desktop_state.cursor_y);
        }
    }
}

int desktop_is_active(void)
{
    return desktop_state.active;
}

unsigned int desktop_work_area_height(void)
{
    if (desktop_state.fb == 0 || desktop_state.fb->height <= desktop_taskbar_height())
    {
        return 0U;
    }

    return desktop_state.fb->height - desktop_taskbar_height();
}

void desktop_destroy_window(desktop_window_t *window)
{
    unsigned int window_index;
    unsigned int index;
    desktop_app_instance_t *owner;

    if (window == 0)
    {
        return;
    }

    window_index = desktop_find_window_index_by_id(window->id);
    if (window_index >= desktop_state.window_count)
    {
        return;
    }

    owner = desktop_state.windows[window_index].owner;
    desktop_notify_window_close(&desktop_state.windows[window_index]);
    for (index = window_index; index + 1U < desktop_state.window_count; index++)
    {
        desktop_state.windows[index] = desktop_state.windows[index + 1U];
    }
    if (desktop_state.window_count > 0U)
    {
        desktop_state.window_count--;
    }
    if (desktop_state.window_count < DESKTOP_MAX_WINDOWS)
    {
        memset(&desktop_state.windows[desktop_state.window_count], 0, sizeof(desktop_state.windows[desktop_state.window_count]));
    }
    desktop_state.drag_window_id = 0U;
    desktop_app_instance_window_closed(owner);
    desktop_recompute_focus();
    desktop_state.dirty = 1;
}

void desktop_minimize_window(desktop_window_t *window)
{
    unsigned int window_index;

    if (window == 0)
    {
        return;
    }

    window_index = desktop_find_window_index_by_id(window->id);
    if (window_index >= desktop_state.window_count)
    {
        return;
    }

    desktop_state.windows[window_index].flags &= ~DESKTOP_WINDOW_FLAG_VISIBLE;
    desktop_state.windows[window_index].flags |= DESKTOP_WINDOW_FLAG_MINIMIZED;
    desktop_state.drag_window_id = 0U;
    desktop_recompute_focus();
    desktop_state.dirty = 1;
}

void desktop_toggle_maximize_window(desktop_window_t *window)
{
    unsigned int window_index;
    desktop_window_t *target;

    if (window == 0 || desktop_state.fb == 0)
    {
        return;
    }

    window_index = desktop_find_window_index_by_id(window->id);
    if (window_index >= desktop_state.window_count)
    {
        return;
    }

    target = &desktop_state.windows[window_index];
    if ((target->flags & DESKTOP_WINDOW_FLAG_MAXIMIZED) == 0U)
    {
        target->restore_x = target->x;
        target->restore_y = target->y;
        target->restore_width = target->width;
        target->restore_height = target->height;
        target->x = 0;
        target->y = 0;
        target->width = desktop_state.fb->width;
        target->height = desktop_work_area_height();
        target->flags |= DESKTOP_WINDOW_FLAG_MAXIMIZED;
    }
    else
    {
        target->x = target->restore_x;
        target->y = target->restore_y;
        target->width = target->restore_width;
        target->height = target->restore_height;
        target->flags &= ~DESKTOP_WINDOW_FLAG_MAXIMIZED;
        desktop_window_clamp_to_screen(target, desktop_state.fb->width, desktop_work_area_height());
    }

    target->flags &= ~DESKTOP_WINDOW_FLAG_MINIMIZED;
    target->flags |= DESKTOP_WINDOW_FLAG_VISIBLE;
    desktop_state.drag_window_id = 0U;
    desktop_state.dirty = 1;
}

int desktop_open_app_window(const char *title)
{
    return desktop_open_custom_window(title, 260U, 124U, 0, 0, 0);
}

int desktop_open_file(const char *path)
{
    const vfs_file_info_t *info;
    const char *extension;

    if (path == 0 || *path == '\0')
    {
        (void)desktop_show_open_error("open failed", "(empty path)");
        return -1;
    }

    info = vfs_stat(path);
    if (info == 0)
    {
        (void)desktop_show_open_error("not found", path);
        return -1;
    }

    if ((info->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        (void)desktop_show_open_error("directory not supported", path);
        return -1;
    }

    extension = desktop_path_extension(path);
    if (desktop_extension_matches(extension, ".txt") ||
        desktop_extension_matches(extension, ".md") ||
        desktop_extension_matches(extension, ".log"))
    {
        if (desktop_launch_app_with_argument("editor", path) == 0)
        {
            return 0;
        }
        (void)desktop_show_open_error("editor launch failed", path);
        return -1;
    }

    (void)desktop_show_open_error("unknown file type", path);
    return -1;
}

int desktop_open_app_instance_window(desktop_app_instance_t *instance,
                                     const char *title,
                                     unsigned int width,
                                     unsigned int height,
                                     void *user_data)
{
    desktop_window_t *window;
    int x;
    int y;

    if (instance == 0 || (instance->flags & DESKTOP_APP_INSTANCE_FLAG_RUNNING) == 0U ||
        title == 0 || *title == '\0')
    {
        return -1;
    }

    if (!desktop_state.initialized && desktop_init() != 0)
    {
        return -1;
    }

    if (desktop_state.window_count >= DESKTOP_MAX_WINDOWS)
    {
        return -1;
    }

    x = 72 + (int)(desktop_state.window_count * 28U);
    y = 88 + (int)(desktop_state.window_count * 24U);

    window = &desktop_state.windows[desktop_state.window_count++];
    desktop_window_init(window,
                        desktop_next_window_id(),
                        x,
                        y,
                        width,
                        height,
                        title);
    window->owner = instance;
    window->user_data = user_data;
    desktop_app_instance_window_opened(instance);
    desktop_window_clamp_to_screen(window, desktop_state.fb->width, desktop_work_area_height());
    desktop_focus_window(window);
    desktop_bring_to_front(window);
    desktop_state.dirty = 1;

    if (desktop_state.active)
    {
        desktop_render();
        if (desktop_display_available())
        {
            (void)virtio_gpu_flush();
        }
    }

    return 0;
}

int desktop_open_custom_window(const char *title,
                               unsigned int width,
                               unsigned int height,
                               desktop_window_draw_fn_t draw,
                               desktop_window_event_fn_t handle_event,
                               void *user_data)
{
    desktop_window_t *window;
    unsigned int index;
    int x;
    int y;

    if (title == 0 || *title == '\0')
    {
        return -1;
    }

    if (!desktop_state.initialized && desktop_init() != 0)
    {
        return -1;
    }

    for (index = 0; index < desktop_state.window_count; index++)
    {
        if (desktop_state.windows[index].owner == 0 &&
            strcmp(desktop_state.windows[index].title, title) == 0)
        {
            desktop_window_bind(&desktop_state.windows[index], draw, handle_event, user_data);
            desktop_focus_window(&desktop_state.windows[index]);
            desktop_bring_to_front(&desktop_state.windows[index]);
            return 0;
        }
    }

    if (desktop_state.window_count >= DESKTOP_MAX_WINDOWS)
    {
        return -1;
    }

    x = 72 + (int)(desktop_state.window_count * 28U);
    y = 88 + (int)(desktop_state.window_count * 24U);

    window = &desktop_state.windows[desktop_state.window_count++];
    desktop_window_init(window,
                        desktop_next_window_id(),
                        x,
                        y,
                        width,
                        height,
                        title);
    desktop_window_bind(window, draw, handle_event, user_data);
    desktop_window_clamp_to_screen(window, desktop_state.fb->width, desktop_work_area_height());
    desktop_focus_window(window);
    desktop_bring_to_front(window);
    desktop_state.dirty = 1;

    if (desktop_state.active)
    {
        desktop_render();
        if (desktop_display_available())
        {
            (void)virtio_gpu_flush();
        }
    }

    return 0;
}

static int desktop_display_available(void)
{
    return desktop_state.fb != 0 && desktop_state.fb->buffer != 0 && virtio_gpu_available();
}

static unsigned int desktop_find_window_index_by_id(unsigned int id)
{
    unsigned int index;

    if (id == 0U)
    {
        return desktop_state.window_count;
    }

    for (index = 0; index < desktop_state.window_count; index++)
    {
        if (desktop_state.windows[index].id == id)
        {
            return index;
        }
    }

    return desktop_state.window_count;
}

static desktop_window_t *desktop_find_window_by_id(unsigned int id)
{
    unsigned int index;

    index = desktop_find_window_index_by_id(id);
    if (index >= desktop_state.window_count)
    {
        return 0;
    }

    return &desktop_state.windows[index];
}

static void desktop_cycle_focus_by_id(unsigned int window_id)
{
    desktop_window_t *window;

    window = desktop_find_window_by_id(window_id);
    if (window == 0)
    {
        return;
    }

    desktop_focus_window(window);
    desktop_bring_to_front(window);
}

static unsigned int desktop_next_window_id(void)
{
    unsigned int highest_id;
    unsigned int index;

    highest_id = 0U;
    for (index = 0; index < desktop_state.window_count; index++)
    {
        if (desktop_state.windows[index].id > highest_id)
        {
            highest_id = desktop_state.windows[index].id;
        }
    }

    return highest_id + 1U;
}

static void desktop_toggle_launcher(void)
{
    unsigned int app_count;

    (void)desktop_list_apps(&app_count);
    desktop_state.launcher_open = !desktop_state.launcher_open;
    if (app_count == 0U)
    {
        desktop_state.launcher_selected_index = 0U;
    }
    else if (desktop_state.launcher_selected_index >= app_count)
    {
        desktop_state.launcher_selected_index = 0U;
    }
    desktop_state.dirty = 1;
}

static void desktop_launcher_move_selection(int delta)
{
    unsigned int app_count;

    (void)desktop_list_apps(&app_count);
    if (app_count == 0U)
    {
        return;
    }

    if (delta < 0)
    {
        if (desktop_state.launcher_selected_index == 0U)
        {
            desktop_state.launcher_selected_index = app_count - 1U;
        }
        else
        {
            desktop_state.launcher_selected_index--;
        }
    }
    else if (delta > 0)
    {
        desktop_state.launcher_selected_index = (desktop_state.launcher_selected_index + 1U) % app_count;
    }

    desktop_state.dirty = 1;
}

static void desktop_launcher_launch_selected(void)
{
    const desktop_app_t *apps;
    unsigned int app_count;

    apps = desktop_list_apps(&app_count);
    if (apps == 0 || app_count == 0U || desktop_state.launcher_selected_index >= app_count)
    {
        return;
    }

    (void)desktop_launch_app(apps[desktop_state.launcher_selected_index].name);
    desktop_state.launcher_open = 0;
    desktop_state.dirty = 1;
}

static void desktop_notify_window_close(desktop_window_t *window)
{
    desktop_event_t event;

    if (window == 0)
    {
        return;
    }

    desktop_event_init(&event);
    event.type = DESKTOP_EVENT_WINDOW_CLOSE;
    event.target_window_id = window->id;
    desktop_dispatch_window_event(window, &event);
}

static void desktop_dispatch_window_event(desktop_window_t *window, const desktop_event_t *event)
{
    if (window == 0 || event == 0)
    {
        return;
    }

    if (window->owner != 0 && window->owner->app != 0 && window->owner->app->on_event != 0)
    {
        window->owner->app->on_event(window->owner, window, event);
        return;
    }

    if (window->handle_event != 0)
    {
        window->handle_event(window, event, window->user_data);
    }
}

static const char *desktop_path_extension(const char *path)
{
    const char *scan;
    const char *last_dot;

    if (path == 0)
    {
        return 0;
    }

    last_dot = 0;
    for (scan = path; *scan != '\0'; scan++)
    {
        if (*scan == '/')
        {
            last_dot = 0;
        }
        else if (*scan == '.')
        {
            last_dot = scan;
        }
    }

    return last_dot;
}

static int desktop_extension_matches(const char *extension, const char *expected)
{
    if (extension == 0 || expected == 0)
    {
        return 0;
    }

    return strcmp(extension, expected) == 0;
}

static void desktop_set_dialog_message(desktop_dialog_state_t *state, const char *prefix, const char *path)
{
    unsigned long used;

    if (state == 0)
    {
        return;
    }

    memset(state->message, 0, sizeof(state->message));
    if (prefix != 0)
    {
        strlcpy(state->message, prefix, sizeof(state->message));
    }

    used = strlen(state->message);
    if (path != 0 && *path != '\0' && used + 2U < sizeof(state->message))
    {
        state->message[used++] = ':';
        state->message[used++] = ' ';
        state->message[used] = '\0';
        strlcpy(state->message + used, path, sizeof(state->message) - used);
    }
}

static int desktop_show_open_error(const char *prefix, const char *path)
{
    desktop_set_dialog_message(&desktop_open_error_dialog, prefix, path);
    return desktop_open_custom_window("Open Error",
                                      320U,
                                      108U,
                                      desktop_dialog_draw,
                                      0,
                                      &desktop_open_error_dialog);
}

static void desktop_recompute_focus(void)
{
    unsigned int index;

    if (desktop_state.window_count == 0U)
    {
        desktop_state.focused_window_index = 0U;
        return;
    }

    for (index = desktop_state.window_count; index > 0U; index--)
    {
        if (desktop_window_is_visible(&desktop_state.windows[index - 1U]))
        {
            desktop_state.focused_window_index = index - 1U;
            return;
        }
    }

    desktop_state.focused_window_index = desktop_state.window_count - 1U;
}
