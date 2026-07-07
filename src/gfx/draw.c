#include "gfx/draw.h"

static int draw_abs(int value)
{
    if (value < 0)
    {
        return -value;
    }

    return value;
}

static int draw_min(int a, int b)
{
    if (a < b)
    {
        return a;
    }

    return b;
}

static int draw_max(int a, int b)
{
    if (a > b)
    {
        return a;
    }

    return b;
}

void draw_pixel(framebuffer_t *fb, int x, int y, unsigned int color)
{
    if (fb == 0 || x < 0 || y < 0)
    {
        return;
    }

    fb_put_pixel(fb, (unsigned int)x, (unsigned int)y, color);
}

void draw_hline(framebuffer_t *fb, int x, int y, int width, unsigned int color)
{
    int start_x;
    int end_x;
    int current_x;

    if (fb == 0 || width <= 0 || y < 0 || y >= (int)fb->height)
    {
        return;
    }

    start_x = draw_max(x, 0);
    end_x = draw_min(x + width, (int)fb->width);
    for (current_x = start_x; current_x < end_x; current_x++)
    {
        fb_put_pixel(fb, (unsigned int)current_x, (unsigned int)y, color);
    }
}

void draw_vline(framebuffer_t *fb, int x, int y, int height, unsigned int color)
{
    int start_y;
    int end_y;
    int current_y;

    if (fb == 0 || height <= 0 || x < 0 || x >= (int)fb->width)
    {
        return;
    }

    start_y = draw_max(y, 0);
    end_y = draw_min(y + height, (int)fb->height);
    for (current_y = start_y; current_y < end_y; current_y++)
    {
        fb_put_pixel(fb, (unsigned int)x, (unsigned int)current_y, color);
    }
}

void draw_line(framebuffer_t *fb, int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx;
    int dy;
    int sx;
    int sy;
    int err;

    if (fb == 0)
    {
        return;
    }

    dx = draw_abs(x1 - x0);
    dy = draw_abs(y1 - y0);
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;

    for (;;)
    {
        draw_pixel(fb, x0, y0, color);

        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        if ((err * 2) > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if ((err * 2) < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(framebuffer_t *fb, int x, int y, int width, int height, unsigned int color)
{
    if (fb == 0 || width <= 0 || height <= 0)
    {
        return;
    }

    draw_hline(fb, x, y, width, color);
    draw_hline(fb, x, y + height - 1, width, color);
    draw_vline(fb, x, y, height, color);
    draw_vline(fb, x + width - 1, y, height, color);
}

void draw_fill_rect(framebuffer_t *fb, int x, int y, int width, int height, unsigned int color)
{
    int start_y;
    int end_y;
    int current_y;

    if (fb == 0 || width <= 0 || height <= 0)
    {
        return;
    }

    start_y = draw_max(y, 0);
    end_y = draw_min(y + height, (int)fb->height);
    for (current_y = start_y; current_y < end_y; current_y++)
    {
        draw_hline(fb, x, current_y, width, color);
    }
}
