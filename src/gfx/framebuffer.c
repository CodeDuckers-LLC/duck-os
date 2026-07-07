#include "gfx/framebuffer.h"
#include "kernel/kmalloc.h"
#include "lib/string.h"

static int fb_is_supported(const framebuffer_t *fb)
{
    if (fb == 0 || fb->buffer == 0)
    {
        return 0;
    }

    return fb->bytes_per_pixel == 4U;
}

framebuffer_t *fb_create_test(unsigned int width, unsigned int height)
{
    framebuffer_t *fb;
    unsigned long pitch;
    unsigned long buffer_size;
    unsigned long allocation_size;

    if (width == 0U || height == 0U)
    {
        return 0;
    }

    pitch = (unsigned long)width * 4UL;
    buffer_size = pitch * (unsigned long)height;
    if (pitch > 0xffffffffUL || buffer_size > 0xffffffffUL)
    {
        return 0;
    }

    allocation_size = sizeof(*fb) + buffer_size;
    if (allocation_size < sizeof(*fb))
    {
        return 0;
    }

    fb = (framebuffer_t *)kzalloc(allocation_size);
    if (fb == 0)
    {
        return 0;
    }

    fb->width = width;
    fb->height = height;
    fb->pitch = (unsigned int)pitch;
    fb->bytes_per_pixel = 4U;
    fb->pixel_format = FB_PIXEL_FORMAT_X8R8G8B8;
    fb->buffer = (unsigned char *)(fb + 1);
    return fb;
}

void fb_clear(framebuffer_t *fb, unsigned int color)
{
    unsigned int x;
    unsigned int y;

    if (!fb_is_supported(fb))
    {
        return;
    }

    for (y = 0; y < fb->height; y++)
    {
        for (x = 0; x < fb->width; x++)
        {
            fb_put_pixel(fb, x, y, color);
        }
    }
}

void fb_put_pixel(framebuffer_t *fb, unsigned int x, unsigned int y, unsigned int color)
{
    unsigned int *row;

    if (!fb_is_supported(fb) || x >= fb->width || y >= fb->height)
    {
        return;
    }

    row = (unsigned int *)(fb->buffer + ((unsigned long)y * fb->pitch));
    row[x] = color;
}

unsigned int fb_get_pixel(const framebuffer_t *fb, unsigned int x, unsigned int y)
{
    const unsigned int *row;

    if (!fb_is_supported(fb) || x >= fb->width || y >= fb->height)
    {
        return 0U;
    }

    row = (const unsigned int *)(fb->buffer + ((unsigned long)y * fb->pitch));
    return row[x];
}
