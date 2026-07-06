#ifndef GRAPHICS_FRAMEBUFFER_H
#define GRAPHICS_FRAMEBUFFER_H

typedef struct framebuffer
{
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    unsigned int bytes_per_pixel;
    unsigned char *pixels;
} framebuffer_t;

void framebuffer_init(framebuffer_t *fb,
                      unsigned int width,
                      unsigned int height,
                      unsigned int stride,
                      unsigned int bytes_per_pixel,
                      void *pixels);
void framebuffer_clear(framebuffer_t *fb, unsigned int color);
void framebuffer_fill_rect(framebuffer_t *fb,
                           unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           unsigned int color);
void framebuffer_draw_checkerboard(framebuffer_t *fb,
                                   unsigned int tile_size,
                                   unsigned int color_a,
                                   unsigned int color_b);
void framebuffer_draw_gradient(framebuffer_t *fb);
void framebuffer_draw_demo(framebuffer_t *fb);

#endif
