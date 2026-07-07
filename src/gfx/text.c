#include "gfx/font.h"
#include "gfx/draw.h"

void gfx_draw_char(framebuffer_t *fb, int x, int y, char ch, unsigned int fg_color, unsigned int bg_color)
{
    const unsigned char *glyph;
    int row;
    int col;

    glyph = gfx_font8x8_get(ch);
    for (row = 0; row < GFX_FONT_HEIGHT; row++)
    {
        for (col = 0; col < GFX_FONT_WIDTH; col++)
        {
            if ((glyph[row] & (1U << col)) != 0U)
            {
                draw_pixel(fb, x + col, y + row, fg_color);
            }
            else
            {
                draw_pixel(fb, x + col, y + row, bg_color);
            }
        }
    }
}

void gfx_draw_string(framebuffer_t *fb, int x, int y, const char *text, unsigned int fg_color, unsigned int bg_color)
{
    int cursor_x;
    int cursor_y;

    if (fb == 0 || text == 0)
    {
        return;
    }

    cursor_x = x;
    cursor_y = y;

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = x;
            cursor_y += GFX_FONT_HEIGHT;
        }
        else
        {
            gfx_draw_char(fb, cursor_x, cursor_y, *text, fg_color, bg_color);
            cursor_x += GFX_FONT_WIDTH;
        }

        text++;
    }
}
