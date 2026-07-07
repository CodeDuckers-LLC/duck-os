#include "drivers/virtio_gpu.h"
#include "gfx/cursor.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gui/gui.h"
#include "lib/string.h"

#define GUI_MAX_WINDOWS 8U
#define GUI_BORDER_COLOR 0xff101010U
#define GUI_TITLE_BG_COLOR 0xff355c7dU
#define GUI_TITLE_FG_COLOR 0xffffffffU
#define GUI_WINDOW_BG_COLOR 0xffd9e2ecU
#define GUI_DESKTOP_BG_COLOR 0xff6c8eadU
#define GUI_TITLE_HEIGHT (GFX_FONT_HEIGHT + 6)
#define GUI_PADDING 4

typedef struct gui_state
{
    framebuffer_t *fb;
    window_t windows[GUI_MAX_WINDOWS];
    unsigned int next_id;
} gui_state_t;

static gui_state_t gui_state;

static int gui_window_is_usable(const window_t *window)
{
    return window->id != 0U && window->visible;
}

static int gui_window_right(const window_t *window)
{
    return window->x + (int)window->width;
}

static int gui_window_bottom(const window_t *window)
{
    return window->y + (int)window->height;
}

static int gui_clip_dimension(int start, unsigned int size, unsigned int limit)
{
    long end;

    if (size == 0U || start >= (int)limit)
    {
        return 0;
    }

    end = (long)start + (long)size;
    if (end <= 0)
    {
        return 0;
    }

    if (end > (long)limit)
    {
        end = (long)limit;
    }

    if (start < 0)
    {
        start = 0;
    }

    return (int)end - start;
}

static void gui_draw_window_chrome(framebuffer_t *fb, window_t *window)
{
    int x;
    int y;
    int width;
    int height;
    int title_height;
    int client_y;
    int client_height;

    x = window->x;
    y = window->y;
    width = gui_clip_dimension(x, window->width, fb->width);
    height = gui_clip_dimension(y, window->height, fb->height);
    if (width <= 0 || height <= 0)
    {
        return;
    }

    title_height = GUI_TITLE_HEIGHT;
    if (title_height > height)
    {
        title_height = height;
    }

    draw_fill_rect(fb, x, y, width, height, GUI_WINDOW_BG_COLOR);
    draw_fill_rect(fb, x, y, width, title_height, GUI_TITLE_BG_COLOR);
    draw_rect(fb, x, y, width, height, GUI_BORDER_COLOR);
    gfx_draw_string(fb,
                    x + GUI_PADDING,
                    y + 3,
                    window->title,
                    GUI_TITLE_FG_COLOR,
                    GUI_TITLE_BG_COLOR);

    client_y = y + title_height;
    client_height = height - title_height;
    if (client_height > 0)
    {
        draw_fill_rect(fb, x + 1, client_y, width - 2, client_height - 1, GUI_WINDOW_BG_COLOR);
    }
}

void gui_attach_framebuffer(framebuffer_t *fb)
{
    gui_state.fb = fb;
}

framebuffer_t *gui_framebuffer(void)
{
    return gui_state.fb;
}

window_t *gui_create_window(int x,
                            int y,
                            unsigned int width,
                            unsigned int height,
                            const char *title,
                            window_draw_fn_t draw)
{
    unsigned int index;
    window_t *window;

    if (width < 16U || height < (unsigned int)(GUI_TITLE_HEIGHT + 4))
    {
        return 0;
    }

    for (index = 0; index < GUI_MAX_WINDOWS; index++)
    {
        if (gui_state.windows[index].id == 0U)
        {
            window = &gui_state.windows[index];
            memset(window, 0, sizeof(*window));
            gui_state.next_id++;
            if (gui_state.next_id == 0U)
            {
                gui_state.next_id++;
            }
            window->id = gui_state.next_id;
            window->x = x;
            window->y = y;
            window->width = width;
            window->height = height;
            window->visible = 1;
            window->draw = draw;
            if (title != 0)
            {
                strlcpy(window->title, title, sizeof(window->title));
            }
            return window;
        }
    }

    return 0;
}

void gui_destroy_window(unsigned int id)
{
    unsigned int index;

    if (id == 0U)
    {
        return;
    }

    for (index = 0; index < GUI_MAX_WINDOWS; index++)
    {
        if (gui_state.windows[index].id == id)
        {
            memset(&gui_state.windows[index], 0, sizeof(gui_state.windows[index]));
            return;
        }
    }
}

void gui_draw_all(void)
{
    unsigned int index;

    if (gui_state.fb == 0 || gui_state.fb->buffer == 0)
    {
        return;
    }

    fb_clear(gui_state.fb, GUI_DESKTOP_BG_COLOR);

    for (index = 0; index < GUI_MAX_WINDOWS; index++)
    {
        window_t *window;

        window = &gui_state.windows[index];
        if (!gui_window_is_usable(window))
        {
            continue;
        }

        if (gui_window_right(window) <= 0 || gui_window_bottom(window) <= 0)
        {
            continue;
        }

        gui_draw_window_chrome(gui_state.fb, window);
        if (window->draw != 0)
        {
            window->draw(window, gui_state.fb);
        }
    }

    gfx_cursor_refresh();
    (void)virtio_gpu_flush();
}
