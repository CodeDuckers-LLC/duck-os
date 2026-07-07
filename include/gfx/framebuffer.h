#ifndef GFX_FRAMEBUFFER_H
#define GFX_FRAMEBUFFER_H

typedef enum fb_pixel_format
{
    FB_PIXEL_FORMAT_X8R8G8B8 = 0,
    FB_PIXEL_FORMAT_B8G8R8A8 = 1
} fb_pixel_format_t;

typedef struct framebuffer
{
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int bytes_per_pixel;
    fb_pixel_format_t pixel_format;
    unsigned char *buffer;
} framebuffer_t;

framebuffer_t *fb_create_test(unsigned int width, unsigned int height);
void fb_clear(framebuffer_t *fb, unsigned int color);
void fb_put_pixel(framebuffer_t *fb, unsigned int x, unsigned int y, unsigned int color);
unsigned int fb_get_pixel(const framebuffer_t *fb, unsigned int x, unsigned int y);

#endif
