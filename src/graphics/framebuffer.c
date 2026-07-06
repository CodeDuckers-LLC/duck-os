#include "graphics/framebuffer.h"

static void framebuffer_put_pixel(framebuffer_t *fb,
                                  unsigned int x,
                                  unsigned int y,
                                  unsigned int color)
{
    unsigned int *row;

    if (fb == 0 || fb->pixels == 0 || x >= fb->width || y >= fb->height || fb->bytes_per_pixel != 4U)
    {
        return;
    }

    row = (unsigned int *)(fb->pixels + (y * fb->stride));
    row[x] = color;
}

void framebuffer_init(framebuffer_t *fb,
                      unsigned int width,
                      unsigned int height,
                      unsigned int stride,
                      unsigned int bytes_per_pixel,
                      void *pixels)
{
    if (fb == 0)
    {
        return;
    }

    fb->width = width;
    fb->height = height;
    fb->stride = stride;
    fb->bytes_per_pixel = bytes_per_pixel;
    fb->pixels = (unsigned char *)pixels;
}

void framebuffer_clear(framebuffer_t *fb, unsigned int color)
{
    unsigned int y;
    unsigned int x;

    if (fb == 0)
    {
        return;
    }

    for (y = 0; y < fb->height; y++)
    {
        for (x = 0; x < fb->width; x++)
        {
            framebuffer_put_pixel(fb, x, y, color);
        }
    }
}

void framebuffer_fill_rect(framebuffer_t *fb,
                           unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           unsigned int color)
{
    unsigned int row;
    unsigned int col;
    unsigned int max_x;
    unsigned int max_y;

    if (fb == 0)
    {
        return;
    }

    max_x = x + width;
    if (max_x > fb->width)
    {
        max_x = fb->width;
    }

    max_y = y + height;
    if (max_y > fb->height)
    {
        max_y = fb->height;
    }

    for (row = y; row < max_y; row++)
    {
        for (col = x; col < max_x; col++)
        {
            framebuffer_put_pixel(fb, col, row, color);
        }
    }
}

void framebuffer_draw_checkerboard(framebuffer_t *fb,
                                   unsigned int tile_size,
                                   unsigned int color_a,
                                   unsigned int color_b)
{
    unsigned int y;
    unsigned int x;

    if (fb == 0 || tile_size == 0U)
    {
        return;
    }

    for (y = 0; y < fb->height; y += tile_size)
    {
        for (x = 0; x < fb->width; x += tile_size)
        {
            unsigned int tile_x;
            unsigned int tile_y;

            tile_x = x / tile_size;
            tile_y = y / tile_size;
            framebuffer_fill_rect(fb,
                                  x,
                                  y,
                                  tile_size,
                                  tile_size,
                                  ((tile_x + tile_y) & 1U) == 0U ? color_a : color_b);
        }
    }
}

void framebuffer_draw_gradient(framebuffer_t *fb)
{
    unsigned int y;
    unsigned int x;

    if (fb == 0 || fb->width == 0U || fb->height == 0U)
    {
        return;
    }

    for (y = 0; y < fb->height; y++)
    {
        for (x = 0; x < fb->width; x++)
        {
            unsigned int blue;
            unsigned int green;
            unsigned int red;

            blue = (x * 255U) / fb->width;
            green = (y * 255U) / fb->height;
            red = ((x + y) * 255U) / (fb->width + fb->height);
            framebuffer_put_pixel(fb, x, y, (0xffU << 24) | (red << 16) | (green << 8) | blue);
        }
    }
}

void framebuffer_draw_demo(framebuffer_t *fb)
{
    unsigned int inset_x;
    unsigned int inset_y;

    if (fb == 0)
    {
        return;
    }

    framebuffer_draw_gradient(fb);
    framebuffer_draw_checkerboard(fb, 32U, 0x10203040U, 0x10304050U);

    inset_x = fb->width / 10U;
    inset_y = fb->height / 10U;
    framebuffer_fill_rect(fb, inset_x, inset_y, fb->width - (2U * inset_x), fb->height / 6U, 0xff204060U);
    framebuffer_fill_rect(fb,
                          inset_x + 16U,
                          inset_y + 16U,
                          (fb->width / 2U),
                          (fb->height / 10U),
                          0xfff2b134U);
    framebuffer_fill_rect(fb,
                          fb->width / 6U,
                          fb->height / 2U,
                          fb->width / 3U,
                          fb->height / 5U,
                          0xff2dce89U);
    framebuffer_fill_rect(fb,
                          fb->width / 2U,
                          fb->height / 3U,
                          fb->width / 4U,
                          fb->height / 3U,
                          0xffd94f70U);
}
