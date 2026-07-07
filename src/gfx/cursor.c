#include "gfx/cursor.h"
#include "gfx/framebuffer.h"

#define GFX_CURSOR_WIDTH 12U
#define GFX_CURSOR_HEIGHT 16U
#define GFX_CURSOR_FILL_COLOR 0xffffffffU
#define GFX_CURSOR_ACCENT_COLOR 0xff7fd1ffU
#define GFX_CURSOR_OUTLINE_COLOR 0xff101010U

typedef struct gfx_cursor_state
{
    framebuffer_t *fb;
    unsigned int x;
    unsigned int y;
    unsigned int saved_width;
    unsigned int saved_height;
    unsigned int saved_pixels[GFX_CURSOR_WIDTH * GFX_CURSOR_HEIGHT];
    int visible;
} gfx_cursor_state_t;

static gfx_cursor_state_t gfx_cursor_state;

static unsigned int gfx_cursor_clip_width(unsigned int x)
{
    unsigned int width;

    width = GFX_CURSOR_WIDTH;
    if (gfx_cursor_state.fb == 0 || x >= gfx_cursor_state.fb->width)
    {
        return 0U;
    }

    if (x + width > gfx_cursor_state.fb->width)
    {
        width = gfx_cursor_state.fb->width - x;
    }

    return width;
}

static unsigned int gfx_cursor_clip_height(unsigned int y)
{
    unsigned int height;

    height = GFX_CURSOR_HEIGHT;
    if (gfx_cursor_state.fb == 0 || y >= gfx_cursor_state.fb->height)
    {
        return 0U;
    }

    if (y + height > gfx_cursor_state.fb->height)
    {
        height = gfx_cursor_state.fb->height - y;
    }

    return height;
}

static void gfx_cursor_restore_background(void)
{
    unsigned int row;
    unsigned int col;

    if (!gfx_cursor_state.visible || gfx_cursor_state.fb == 0)
    {
        return;
    }

    for (row = 0; row < gfx_cursor_state.saved_height; row++)
    {
        for (col = 0; col < gfx_cursor_state.saved_width; col++)
        {
            fb_put_pixel(gfx_cursor_state.fb,
                         gfx_cursor_state.x + col,
                         gfx_cursor_state.y + row,
                         gfx_cursor_state.saved_pixels[(row * GFX_CURSOR_WIDTH) + col]);
        }
    }

    gfx_cursor_state.visible = 0;
}

static void gfx_cursor_save_background(unsigned int x, unsigned int y)
{
    unsigned int row;
    unsigned int col;

    gfx_cursor_state.saved_width = gfx_cursor_clip_width(x);
    gfx_cursor_state.saved_height = gfx_cursor_clip_height(y);

    for (row = 0; row < gfx_cursor_state.saved_height; row++)
    {
        for (col = 0; col < gfx_cursor_state.saved_width; col++)
        {
            gfx_cursor_state.saved_pixels[(row * GFX_CURSOR_WIDTH) + col] =
                fb_get_pixel(gfx_cursor_state.fb, x + col, y + row);
        }
    }
}

static void gfx_cursor_draw_at(unsigned int x, unsigned int y)
{
    unsigned int row;
    unsigned int col;

    for (row = 0; row < gfx_cursor_state.saved_height; row++)
    {
        for (col = 0; col < gfx_cursor_state.saved_width; col++)
        {
            unsigned int color;

            color = 0U;
            if (row == 0U || col == 0U || row + 1U == gfx_cursor_state.saved_height || col + 1U == gfx_cursor_state.saved_width)
            {
                color = GFX_CURSOR_OUTLINE_COLOR;
            }
            else if (row >= 3U && row + 3U < gfx_cursor_state.saved_height &&
                     col >= 3U && col + 3U < gfx_cursor_state.saved_width)
            {
                color = GFX_CURSOR_FILL_COLOR;
            }
            else
            {
                color = GFX_CURSOR_ACCENT_COLOR;
            }

            if (color != 0U)
            {
                fb_put_pixel(gfx_cursor_state.fb, x + col, y + row, color);
            }
        }
    }

    gfx_cursor_state.visible = 1;
}

void gfx_cursor_attach(framebuffer_t *fb)
{
    if (gfx_cursor_state.visible)
    {
        gfx_cursor_restore_background();
    }

    gfx_cursor_state.fb = fb;
    gfx_cursor_state.x = 0U;
    gfx_cursor_state.y = 0U;
    gfx_cursor_state.saved_width = 0U;
    gfx_cursor_state.saved_height = 0U;
    gfx_cursor_state.visible = 0;
}

int gfx_cursor_available(void)
{
    return gfx_cursor_state.fb != 0 && gfx_cursor_state.fb->buffer != 0;
}

void gfx_cursor_move(unsigned int x, unsigned int y)
{
    if (!gfx_cursor_available())
    {
        return;
    }

    gfx_cursor_restore_background();

    if (x >= gfx_cursor_state.fb->width)
    {
        x = gfx_cursor_state.fb->width - 1U;
    }
    if (y >= gfx_cursor_state.fb->height)
    {
        y = gfx_cursor_state.fb->height - 1U;
    }

    gfx_cursor_state.x = x;
    gfx_cursor_state.y = y;
    gfx_cursor_save_background(x, y);
    gfx_cursor_draw_at(x, y);
}

void gfx_cursor_refresh(void)
{
    unsigned int x;
    unsigned int y;

    if (!gfx_cursor_available())
    {
        return;
    }

    x = gfx_cursor_state.x;
    y = gfx_cursor_state.y;
    gfx_cursor_state.visible = 0;
    gfx_cursor_save_background(x, y);
    gfx_cursor_draw_at(x, y);
}

unsigned int gfx_cursor_x(void)
{
    return gfx_cursor_state.x;
}

unsigned int gfx_cursor_y(void)
{
    return gfx_cursor_state.y;
}
