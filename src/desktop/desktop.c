#include "desktop/desktop.h"
#include "desktop/desktop_app.h"
#include "desktop/desktop_event.h"
#include "desktop/desktop_input.h"
#include "desktop/desktop_window.h"
#include "arch/aarch64/cpu.h"
#include "drivers/virtio_gpu.h"
#include "gfx/cursor.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "kernel/console.h"
#include "lib/string.h"

#define DESKTOP_MAX_WINDOWS 4U
#define DESKTOP_MAX_APPS 4U
#define DESKTOP_BG_COLOR 0xff6d8fa4U
#define DESKTOP_ACCENT_COLOR 0xff89aebfU
#define DESKTOP_TASKBAR_COLOR 0xff20313cU
#define DESKTOP_TASKBAR_TEXT_COLOR 0xffffffffU
#define DESKTOP_TASKBAR_HEIGHT 24
#define DESKTOP_START_BG_COLOR 0xff2e596bU
#define DESKTOP_PANEL_COLOR 0xff7ea2b3U
#define DESKTOP_TEXT_COLOR 0xff102030U

typedef struct desktop_state
{
    framebuffer_t *fb;
    desktop_window_t windows[DESKTOP_MAX_WINDOWS];
    desktop_app_t apps[DESKTOP_MAX_APPS];
    unsigned int window_count;
    unsigned int app_count;
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
    int initialized;
    int active;
    int dirty;
} desktop_state_t;

static desktop_state_t desktop_state;

static void desktop_create_layout(void);
static void desktop_handle_event(const desktop_event_t *event);
static void desktop_draw_background(framebuffer_t *fb);
static void desktop_draw_taskbar(framebuffer_t *fb);
static void desktop_draw_window_contents(framebuffer_t *fb, const desktop_window_t *window);
static int desktop_display_available(void);
static unsigned int desktop_find_window_index_by_id(unsigned int id);
static desktop_window_t *desktop_find_window_by_id(unsigned int id);
static void desktop_bring_window_to_front(unsigned int window_index);

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
    desktop_window_init(&desktop_state.windows[desktop_state.window_count++],
                        3U,
                        120,
                        188,
                        320U,
                        104U,
                        "Applications");

    desktop_state.app_count = 0U;
    desktop_app_init(&desktop_state.apps[desktop_state.app_count++], 1U, "desktop");
}

static void desktop_handle_event(const desktop_event_t *event)
{
    if (event == 0)
    {
        return;
    }

    switch (event->type)
    {
    case DESKTOP_EVENT_REDRAW:
    case DESKTOP_EVENT_KEY:
    case DESKTOP_EVENT_CHAR:
        desktop_state.dirty = 1;
        break;
    case DESKTOP_EVENT_BUTTON_DOWN:
    {
        unsigned int window_index;
        desktop_window_t *window;

        window_index = desktop_find_window_index_by_id(event->target_window_id);
        if (window_index < desktop_state.window_count)
        {
            desktop_bring_window_to_front(window_index);
            desktop_state.focused_window_index = desktop_state.window_count - 1U;
            window = &desktop_state.windows[desktop_state.focused_window_index];

            if (desktop_window_title_hit_test(window, event->cursor_x, event->cursor_y))
            {
                desktop_state.drag_window_id = window->id;
                desktop_state.drag_offset_x = event->cursor_x - (unsigned int)window->x;
                desktop_state.drag_offset_y = event->cursor_y - (unsigned int)window->y;
            }
        }
        desktop_state.dirty = 1;
        break;
    }
    case DESKTOP_EVENT_BUTTON_UP:
        desktop_state.primary_button_down = 0U;
        desktop_state.drag_window_id = 0U;
        desktop_state.dirty = 1;
        break;
    case DESKTOP_EVENT_CURSOR_MOVE:
    {
        desktop_window_t *window;

        window = desktop_find_window_by_id(desktop_state.drag_window_id);
        if (window != 0)
        {
            window->x = (int)event->cursor_x - (int)desktop_state.drag_offset_x;
            window->y = (int)event->cursor_y - (int)desktop_state.drag_offset_y;
            desktop_window_clamp_to_screen(window, desktop_state.fb->width, desktop_state.fb->height);
        }
        desktop_state.dirty = 1;
        break;
    }
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
                       (int)fb->height - DESKTOP_TASKBAR_HEIGHT - 52,
                       DESKTOP_PANEL_COLOR);
    }

    gfx_draw_string(fb, 24, 18, "duck-os desktop", 0xffffffffU, DESKTOP_ACCENT_COLOR);
    gfx_draw_string(fb,
                    (int)fb->width - 176,
                    18,
                    "Tab focus  Enter click",
                    0xffffffffU,
                    DESKTOP_ACCENT_COLOR);
}

static void desktop_draw_taskbar(framebuffer_t *fb)
{
    int taskbar_y;

    taskbar_y = (int)fb->height - DESKTOP_TASKBAR_HEIGHT;
    draw_fill_rect(fb, 0, taskbar_y, (int)fb->width, DESKTOP_TASKBAR_HEIGHT, DESKTOP_TASKBAR_COLOR);
    draw_fill_rect(fb, 8, taskbar_y + 4, 68, DESKTOP_TASKBAR_HEIGHT - 8, DESKTOP_START_BG_COLOR);
    draw_rect(fb, 8, taskbar_y + 4, 68, DESKTOP_TASKBAR_HEIGHT - 8, 0xffb9d7e3U);
    gfx_draw_string(fb, 24, taskbar_y + 8, "Start", DESKTOP_TASKBAR_TEXT_COLOR, DESKTOP_START_BG_COLOR);
    gfx_draw_string(fb,
                    96,
                    taskbar_y + 8,
                    "Esc exit  Arrows/WASD move  Enter click",
                    DESKTOP_TASKBAR_TEXT_COLOR,
                    DESKTOP_TASKBAR_COLOR);
    gfx_draw_string(fb,
                    (int)fb->width - 152,
                    taskbar_y + 8,
                    "kernel-mode desktop",
                    DESKTOP_TASKBAR_TEXT_COLOR,
                    DESKTOP_TASKBAR_COLOR);
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

    if (window->id == 1U)
    {
        gfx_draw_string(fb, x + 8, y + 10, "Desktop runtime online", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 26, "Title bars can be dragged", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 42, "Press Escape to exit", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
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
        gfx_draw_string(fb, x + 8, y + 10, "Built-in apps", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 26, "- file browser: pending", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 42, "- text viewer: pending", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
        gfx_draw_string(fb, x + 8, y + 58, "Focus follows top window", DESKTOP_TEXT_COLOR, 0xffeef4f7U);
    }
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
    unsigned int index;

    if (!desktop_state.initialized || desktop_state.fb == 0 || desktop_state.fb->buffer == 0)
    {
        return;
    }

    desktop_draw_background(desktop_state.fb);

    for (index = 0; index < desktop_state.window_count; index++)
    {
        desktop_window_draw(&desktop_state.windows[index],
                            desktop_state.fb,
                            index == desktop_state.focused_window_index);
        desktop_draw_window_contents(desktop_state.fb, &desktop_state.windows[index]);
    }

    desktop_draw_taskbar(desktop_state.fb);
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

static void desktop_bring_window_to_front(unsigned int window_index)
{
    desktop_window_t window;
    unsigned int index;

    if (window_index >= desktop_state.window_count || window_index + 1U == desktop_state.window_count)
    {
        return;
    }

    window = desktop_state.windows[window_index];
    for (index = window_index; index + 1U < desktop_state.window_count; index++)
    {
        desktop_state.windows[index] = desktop_state.windows[index + 1U];
    }
    desktop_state.windows[desktop_state.window_count - 1U] = window;
}
