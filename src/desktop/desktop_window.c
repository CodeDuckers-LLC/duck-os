#include "desktop/desktop_window.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "lib/string.h"

#define DESKTOP_WINDOW_BORDER_COLOR 0xff16313dU
#define DESKTOP_WINDOW_ACTIVE_TITLE_BG 0xff2a6076U
#define DESKTOP_WINDOW_INACTIVE_TITLE_BG 0xff546b78U
#define DESKTOP_WINDOW_TITLE_FG 0xffffffffU
#define DESKTOP_WINDOW_BODY_BG 0xffd9e4eaU
#define DESKTOP_WINDOW_TITLE_HEIGHT (GFX_FONT_HEIGHT + 8)
#define DESKTOP_WINDOW_PADDING 8
#define DESKTOP_WINDOW_CONTROL_SIZE 12
#define DESKTOP_WINDOW_CONTROL_GAP 4
#define DESKTOP_WINDOW_CONTROL_MARGIN 6
#define DESKTOP_WINDOW_CONTROL_CLOSE_BG 0xffd76464U
#define DESKTOP_WINDOW_CONTROL_MINIMIZE_BG 0xffd8b45cU
#define DESKTOP_WINDOW_CONTROL_MAXIMIZE_BG 0xff6db27aU

static int desktop_window_control_x(const desktop_window_t *window, unsigned int control)
{
    int right;
    int offset;

    if (window == 0 || control == DESKTOP_WINDOW_CONTROL_NONE)
    {
        return 0;
    }

    right = window->x + (int)window->width - DESKTOP_WINDOW_CONTROL_MARGIN - DESKTOP_WINDOW_CONTROL_SIZE;
    offset = (int)(control - 1U) * (DESKTOP_WINDOW_CONTROL_SIZE + DESKTOP_WINDOW_CONTROL_GAP);
    return right - offset;
}

static int desktop_clip_dimension(int start, unsigned int size, unsigned int limit)
{
    long end;

    if (size == 0U || start >= (int)limit)
    {
        return 0;
    }

    end = (long)start + (long)size;
    if (end <= 0L)
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

static int desktop_window_right(const desktop_window_t *window)
{
    return window->x + (int)window->width;
}

static int desktop_window_bottom(const desktop_window_t *window)
{
    return window->y + (int)window->height;
}

void desktop_window_init(desktop_window_t *window,
                         unsigned int id,
                         int x,
                         int y,
                         unsigned int width,
                         unsigned int height,
                         const char *title)
{
    if (window == 0)
    {
        return;
    }

    memset(window, 0, sizeof(*window));
    window->id = id;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->flags = DESKTOP_WINDOW_FLAG_VISIBLE;
    window->restore_x = x;
    window->restore_y = y;
    window->restore_width = width;
    window->restore_height = height;
    if (title != 0)
    {
        strlcpy(window->title, title, sizeof(window->title));
    }
}

void desktop_window_bind(desktop_window_t *window,
                         desktop_window_draw_fn_t draw,
                         desktop_window_event_fn_t handle_event,
                         void *user_data)
{
    if (window == 0)
    {
        return;
    }

    window->draw = draw;
    window->handle_event = handle_event;
    window->user_data = user_data;
}

int desktop_window_is_visible(const desktop_window_t *window)
{
    return window != 0 && window->id != 0U && (window->flags & DESKTOP_WINDOW_FLAG_VISIBLE) != 0U;
}

int desktop_window_content_x(const desktop_window_t *window)
{
    if (window == 0)
    {
        return 0;
    }

    return window->x + DESKTOP_WINDOW_PADDING;
}

int desktop_window_content_y(const desktop_window_t *window)
{
    if (window == 0)
    {
        return 0;
    }

    return window->y + DESKTOP_WINDOW_TITLE_HEIGHT + DESKTOP_WINDOW_PADDING;
}

unsigned int desktop_window_content_width(const desktop_window_t *window)
{
    if (window == 0 || window->width <= (DESKTOP_WINDOW_PADDING * 2U))
    {
        return 0U;
    }

    return window->width - (DESKTOP_WINDOW_PADDING * 2U);
}

unsigned int desktop_window_content_height(const desktop_window_t *window)
{
    unsigned int reserved_height;

    if (window == 0)
    {
        return 0U;
    }

    reserved_height = (unsigned int)DESKTOP_WINDOW_TITLE_HEIGHT + (DESKTOP_WINDOW_PADDING * 2U);
    if (window->height <= reserved_height)
    {
        return 0U;
    }

    return window->height - reserved_height;
}

unsigned int desktop_window_title_height(void)
{
    return (unsigned int)DESKTOP_WINDOW_TITLE_HEIGHT;
}

int desktop_window_contains_point(const desktop_window_t *window, unsigned int x, unsigned int y)
{
    if (!desktop_window_is_visible(window))
    {
        return 0;
    }

    if (window->x < 0 && x < (unsigned int)(-window->x))
    {
        return 0;
    }

    if (window->y < 0 && y < (unsigned int)(-window->y))
    {
        return 0;
    }

    return x >= (unsigned int)window->x &&
           y >= (unsigned int)window->y &&
           x < (unsigned int)desktop_window_right(window) &&
           y < (unsigned int)desktop_window_bottom(window);
}

int desktop_window_title_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y)
{
    unsigned int title_height;

    if (!desktop_window_contains_point(window, x, y))
    {
        return 0;
    }

    title_height = desktop_window_title_height();
    return y < (unsigned int)(window->y + (int)title_height) &&
           desktop_window_control_hit_test(window, x, y) == DESKTOP_WINDOW_CONTROL_NONE;
}

unsigned int desktop_window_control_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y)
{
    unsigned int control;
    unsigned int title_height;

    if (!desktop_window_contains_point(window, x, y))
    {
        return DESKTOP_WINDOW_CONTROL_NONE;
    }

    title_height = desktop_window_title_height();
    if (y >= (unsigned int)(window->y + (int)title_height))
    {
        return DESKTOP_WINDOW_CONTROL_NONE;
    }

    for (control = DESKTOP_WINDOW_CONTROL_CLOSE; control <= DESKTOP_WINDOW_CONTROL_MAXIMIZE; control++)
    {
        int control_x;
        int control_y;

        control_x = desktop_window_control_x(window, control);
        control_y = window->y + ((int)title_height - DESKTOP_WINDOW_CONTROL_SIZE) / 2;
        if (x >= (unsigned int)control_x &&
            x < (unsigned int)(control_x + DESKTOP_WINDOW_CONTROL_SIZE) &&
            y >= (unsigned int)control_y &&
            y < (unsigned int)(control_y + DESKTOP_WINDOW_CONTROL_SIZE))
        {
            return control;
        }
    }

    return DESKTOP_WINDOW_CONTROL_NONE;
}

void desktop_window_clamp_to_screen(desktop_window_t *window, unsigned int screen_width, unsigned int screen_height)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;

    if (window == 0)
    {
        return;
    }

    min_x = -(int)window->width + 24;
    min_y = 0;
    max_x = (int)screen_width - 24;
    max_y = (int)screen_height - (int)desktop_window_title_height();

    if (window->x < min_x)
    {
        window->x = min_x;
    }
    if (window->y < min_y)
    {
        window->y = min_y;
    }
    if (window->x > max_x)
    {
        window->x = max_x;
    }
    if (window->y > max_y)
    {
        window->y = max_y;
    }
}

void desktop_window_draw(const desktop_window_t *window, framebuffer_t *fb, int active)
{
    int width;
    int height;
    int title_height;
    unsigned int title_bg;

    if (!desktop_window_is_visible(window) || fb == 0 || fb->buffer == 0)
    {
        return;
    }

    width = desktop_clip_dimension(window->x, window->width, fb->width);
    height = desktop_clip_dimension(window->y, window->height, fb->height);
    if (width <= 0 || height <= 0)
    {
        return;
    }

    title_height = DESKTOP_WINDOW_TITLE_HEIGHT;
    if (title_height > height)
    {
        title_height = height;
    }

    title_bg = active ? DESKTOP_WINDOW_ACTIVE_TITLE_BG : DESKTOP_WINDOW_INACTIVE_TITLE_BG;

    draw_fill_rect(fb, window->x, window->y, width, height, DESKTOP_WINDOW_BODY_BG);
    draw_fill_rect(fb, window->x, window->y, width, title_height, title_bg);
    draw_rect(fb, window->x, window->y, width, height, DESKTOP_WINDOW_BORDER_COLOR);
    gfx_draw_string(fb,
                    window->x + DESKTOP_WINDOW_PADDING,
                    window->y + 4,
                    window->title,
                    DESKTOP_WINDOW_TITLE_FG,
                    title_bg);
    {
        unsigned int control;

        for (control = DESKTOP_WINDOW_CONTROL_CLOSE; control <= DESKTOP_WINDOW_CONTROL_MAXIMIZE; control++)
        {
            int control_x;
            int control_y;
            unsigned int bg;
            const char *label;

            control_x = desktop_window_control_x(window, control);
            control_y = window->y + (DESKTOP_WINDOW_TITLE_HEIGHT - DESKTOP_WINDOW_CONTROL_SIZE) / 2;
            if (control == DESKTOP_WINDOW_CONTROL_CLOSE)
            {
                bg = DESKTOP_WINDOW_CONTROL_CLOSE_BG;
                label = "X";
            }
            else if (control == DESKTOP_WINDOW_CONTROL_MINIMIZE)
            {
                bg = DESKTOP_WINDOW_CONTROL_MINIMIZE_BG;
                label = "_";
            }
            else
            {
                bg = DESKTOP_WINDOW_CONTROL_MAXIMIZE_BG;
                label = (window->flags & DESKTOP_WINDOW_FLAG_MAXIMIZED) != 0U ? "R" : "+";
            }

            draw_fill_rect(fb, control_x, control_y, DESKTOP_WINDOW_CONTROL_SIZE, DESKTOP_WINDOW_CONTROL_SIZE, bg);
            draw_rect(fb, control_x, control_y, DESKTOP_WINDOW_CONTROL_SIZE, DESKTOP_WINDOW_CONTROL_SIZE, DESKTOP_WINDOW_BORDER_COLOR);
            gfx_draw_string(fb, control_x + 3, control_y + 2, label, DESKTOP_WINDOW_TITLE_FG, bg);
        }
    }
}
