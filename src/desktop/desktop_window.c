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
    if (title != 0)
    {
        strlcpy(window->title, title, sizeof(window->title));
    }
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
    return y < (unsigned int)(window->y + (int)title_height);
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
}
