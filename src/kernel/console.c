#include "drivers/uart.h"
#include "drivers/virtio_gpu.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "kernel/console.h"
#include "kernel/input.h"
#include "kernel/kmalloc.h"
#include "kernel/spinlock.h"
#include "lib/string.h"

static spinlock_t console_lock;
static unsigned int console_mode = CONSOLE_SINK_SERIAL;
static console_output_capture_fn_t console_capture_callback;
static void *console_capture_user_data;
static int console_is_backspace(char c);

typedef struct gfx_console_state
{
    framebuffer_t *fb;
    unsigned char *cells;
    unsigned int cols;
    unsigned int rows;
    unsigned int cursor_col;
    unsigned int cursor_row;
    unsigned int fg_color;
    unsigned int bg_color;
    unsigned int cursor_color;
    unsigned int dirty_x0;
    unsigned int dirty_y0;
    unsigned int dirty_x1;
    unsigned int dirty_y1;
    int dirty_valid;
    int cursor_visible;
} gfx_console_state_t;

static gfx_console_state_t gfx_console;

static void console_flush_unlocked(void)
{
    if ((console_mode & CONSOLE_SINK_GRAPHICS) != 0U && virtio_gpu_available())
    {
        if (gfx_console.dirty_valid)
        {
            (void)virtio_gpu_flush_rect(gfx_console.dirty_x0,
                                        gfx_console.dirty_y0,
                                        gfx_console.dirty_x1 - gfx_console.dirty_x0,
                                        gfx_console.dirty_y1 - gfx_console.dirty_y0);
            gfx_console.dirty_valid = 0;
        }
    }
}

static void gfx_console_mark_dirty_rect(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    unsigned int x1;
    unsigned int y1;

    if (gfx_console.fb == 0 || width == 0U || height == 0U)
    {
        return;
    }

    if (x >= gfx_console.fb->width || y >= gfx_console.fb->height)
    {
        return;
    }

    x1 = x + width;
    y1 = y + height;
    if (x1 > gfx_console.fb->width)
    {
        x1 = gfx_console.fb->width;
    }
    if (y1 > gfx_console.fb->height)
    {
        y1 = gfx_console.fb->height;
    }

    if (!gfx_console.dirty_valid)
    {
        gfx_console.dirty_x0 = x;
        gfx_console.dirty_y0 = y;
        gfx_console.dirty_x1 = x1;
        gfx_console.dirty_y1 = y1;
        gfx_console.dirty_valid = 1;
        return;
    }

    if (x < gfx_console.dirty_x0)
    {
        gfx_console.dirty_x0 = x;
    }
    if (y < gfx_console.dirty_y0)
    {
        gfx_console.dirty_y0 = y;
    }
    if (x1 > gfx_console.dirty_x1)
    {
        gfx_console.dirty_x1 = x1;
    }
    if (y1 > gfx_console.dirty_y1)
    {
        gfx_console.dirty_y1 = y1;
    }
}

static unsigned int gfx_console_index(unsigned int col, unsigned int row)
{
    return row * gfx_console.cols + col;
}

static unsigned char gfx_console_cell_char(unsigned int col, unsigned int row)
{
    unsigned char ch;

    ch = gfx_console.cells[gfx_console_index(col, row)];
    if (ch == 0U)
    {
        return ' ';
    }

    return ch;
}

static void gfx_console_draw_cell(unsigned int col, unsigned int row)
{
    unsigned int x;
    unsigned int y;

    if (gfx_console.fb == 0 || gfx_console.cells == 0 || col >= gfx_console.cols || row >= gfx_console.rows)
    {
        return;
    }

    x = col * GFX_FONT_WIDTH;
    y = row * GFX_FONT_HEIGHT;
    gfx_draw_char(gfx_console.fb,
                  (int)x,
                  (int)y,
                  (char)gfx_console_cell_char(col, row),
                  gfx_console.fg_color,
                  gfx_console.bg_color);
    gfx_console_mark_dirty_rect(x, y, GFX_FONT_WIDTH, GFX_FONT_HEIGHT);
}

static void gfx_console_hide_cursor(void)
{
    if (gfx_console.fb == 0 || gfx_console.cells == 0 || !gfx_console.cursor_visible)
    {
        return;
    }

    gfx_console_draw_cell(gfx_console.cursor_col, gfx_console.cursor_row);
    gfx_console.cursor_visible = 0;
}

static void gfx_console_show_cursor(void)
{
    if (gfx_console.fb == 0 || gfx_console.cells == 0 || gfx_console.cursor_row >= gfx_console.rows || gfx_console.cursor_col >= gfx_console.cols)
    {
        return;
    }

    draw_fill_rect(gfx_console.fb,
                   (int)(gfx_console.cursor_col * GFX_FONT_WIDTH),
                   (int)((gfx_console.cursor_row * GFX_FONT_HEIGHT) + (GFX_FONT_HEIGHT - 2)),
                   GFX_FONT_WIDTH,
                   2,
                   gfx_console.cursor_color);
    gfx_console_mark_dirty_rect(gfx_console.cursor_col * GFX_FONT_WIDTH,
                                (gfx_console.cursor_row * GFX_FONT_HEIGHT) + (GFX_FONT_HEIGHT - 2U),
                                GFX_FONT_WIDTH,
                                2U);
    gfx_console.cursor_visible = 1;
}

static void gfx_console_redraw_all(void)
{
    unsigned int row;
    unsigned int col;

    if (gfx_console.fb == 0 || gfx_console.cells == 0)
    {
        return;
    }

    for (row = 0; row < gfx_console.rows; row++)
    {
        for (col = 0; col < gfx_console.cols; col++)
        {
            gfx_console_draw_cell(col, row);
        }
    }
}

static void gfx_console_scroll(void)
{
    unsigned int row;
    unsigned int col;

    if (gfx_console.fb == 0 || gfx_console.cells == 0 || gfx_console.rows == 0U)
    {
        return;
    }

    for (row = 1; row < gfx_console.rows; row++)
    {
        for (col = 0; col < gfx_console.cols; col++)
        {
            gfx_console.cells[gfx_console_index(col, row - 1U)] =
                gfx_console.cells[gfx_console_index(col, row)];
        }
    }

    row = gfx_console.rows - 1U;
    for (col = 0; col < gfx_console.cols; col++)
    {
        gfx_console.cells[gfx_console_index(col, row)] = ' ';
    }

    gfx_console_redraw_all();
    gfx_console_mark_dirty_rect(0U,
                                0U,
                                gfx_console.cols * GFX_FONT_WIDTH,
                                gfx_console.rows * GFX_FONT_HEIGHT);
}

static void gfx_console_newline(void)
{
    gfx_console.cursor_col = 0U;
    gfx_console.cursor_row++;
    if (gfx_console.cursor_row >= gfx_console.rows)
    {
        gfx_console.cursor_row = gfx_console.rows - 1U;
        gfx_console_scroll();
    }
}

static void gfx_console_backspace(void)
{
    if (gfx_console.fb == 0 || gfx_console.cells == 0)
    {
        return;
    }

    if (gfx_console.cursor_col > 0U)
    {
        gfx_console.cursor_col--;
    }
    else if (gfx_console.cursor_row > 0U)
    {
        gfx_console.cursor_row--;
        gfx_console.cursor_col = gfx_console.cols - 1U;
    }
    else
    {
        return;
    }

    gfx_console.cells[gfx_console_index(gfx_console.cursor_col, gfx_console.cursor_row)] = ' ';
    gfx_console_draw_cell(gfx_console.cursor_col, gfx_console.cursor_row);
}

static void gfx_console_putc(char c)
{
    if (gfx_console.fb == 0 || gfx_console.cells == 0 || gfx_console.cols == 0U || gfx_console.rows == 0U)
    {
        return;
    }

    gfx_console_hide_cursor();

    if (c == '\r')
    {
        gfx_console.cursor_col = 0U;
        gfx_console_show_cursor();
        return;
    }

    if (c == '\n')
    {
        gfx_console_newline();
        gfx_console_show_cursor();
        return;
    }

    if (console_is_backspace(c))
    {
        gfx_console_backspace();
        gfx_console_show_cursor();
        return;
    }

    if ((unsigned char)c < 32U || (unsigned char)c > 126U)
    {
        c = '?';
    }

    gfx_console.cells[gfx_console_index(gfx_console.cursor_col, gfx_console.cursor_row)] = (unsigned char)c;
    gfx_console_draw_cell(gfx_console.cursor_col, gfx_console.cursor_row);
    gfx_console.cursor_col++;
    if (gfx_console.cursor_col >= gfx_console.cols)
    {
        gfx_console_newline();
    }

    gfx_console_show_cursor();
}

static int console_is_backspace(char c)
{
    return c == '\b' || c == 0x7f;
}

void console_init(void)
{
    memset(&gfx_console, 0, sizeof(gfx_console));
    console_mode = CONSOLE_SINK_SERIAL;
}

void console_putc_unlocked(char c)
{
    if (console_capture_callback != 0)
    {
        console_capture_callback(c, console_capture_user_data);
        return;
    }

    if ((console_mode & CONSOLE_SINK_SERIAL) != 0U)
    {
        if (c == '\n')
        {
            uart_putc('\r');
        }

        uart_putc(c);
    }

    if ((console_mode & CONSOLE_SINK_GRAPHICS) != 0U)
    {
        gfx_console_putc(c);
    }
}

void console_write_unlocked(const char *str)
{
    while (*str != '\0')
    {
        console_putc_unlocked(*str);
        str++;
    }
}

void console_putc(char c)
{
    unsigned long flags;

    flags = spin_lock_irqsave(&console_lock);
    console_putc_unlocked(c);
    console_flush_unlocked();
    spin_unlock_irqrestore(&console_lock, flags);
}

void console_write(const char *str)
{
    unsigned long flags;

    flags = spin_lock_irqsave(&console_lock);
    console_write_unlocked(str);
    console_flush_unlocked();
    spin_unlock_irqrestore(&console_lock, flags);
}

void console_write_len(const char *buffer, unsigned long length)
{
    unsigned long flags;
    unsigned long i;

    flags = spin_lock_irqsave(&console_lock);
    for (i = 0; i < length; i++)
    {
        console_putc_unlocked(buffer[i]);
    }
    console_flush_unlocked();
    spin_unlock_irqrestore(&console_lock, flags);
}

void console_flush(void)
{
    unsigned long flags;

    flags = spin_lock_irqsave(&console_lock);
    console_flush_unlocked();
    spin_unlock_irqrestore(&console_lock, flags);
}

unsigned long console_read_line(char *buffer, unsigned long max_len)
{
    input_event_t event;
    unsigned long length;

    if (max_len == 0)
    {
        return 0;
    }

    length = 0;

    while (1)
    {
        char c;

        while (!input_has_event())
        {
            input_poll();
        }

        if (!input_pop_event(&event) || !input_event_to_char(&event, &c))
        {
            continue;
        }

        if (c == '\r' || c == '\n')
        {
            console_putc('\n');
            buffer[length] = '\0';
            return length;
        }

        if (console_is_backspace(c))
        {
            if (length != 0)
            {
                length--;
                console_write("\b \b");
            }
            continue;
        }

        if (length + 1 < max_len)
        {
            buffer[length] = c;
            length++;
            console_putc(c);
        }
    }
}

void console_attach_graphics(framebuffer_t *fb)
{
    unsigned long flags;
    unsigned int cell_count;
    unsigned char *cells;

    flags = spin_lock_irqsave(&console_lock);

    gfx_console_hide_cursor();
    memset(&gfx_console, 0, sizeof(gfx_console));

    if (fb == 0 || fb->width < GFX_FONT_WIDTH || fb->height < GFX_FONT_HEIGHT)
    {
        console_mode &= ~CONSOLE_SINK_GRAPHICS;
        spin_unlock_irqrestore(&console_lock, flags);
        return;
    }

    gfx_console.fb = fb;
    gfx_console.cols = fb->width / GFX_FONT_WIDTH;
    gfx_console.rows = fb->height / GFX_FONT_HEIGHT;
    cell_count = gfx_console.cols * gfx_console.rows;
    cells = (unsigned char *)kzalloc(cell_count);
    if (cells == 0)
    {
        memset(&gfx_console, 0, sizeof(gfx_console));
        console_mode &= ~CONSOLE_SINK_GRAPHICS;
        spin_unlock_irqrestore(&console_lock, flags);
        return;
    }

    gfx_console.cells = cells;
    gfx_console.fg_color = 0xffffffffU;
    gfx_console.bg_color = 0xff000000U;
    gfx_console.cursor_color = 0xff7fd1ffU;
    fb_clear(fb, gfx_console.bg_color);
    gfx_console_mark_dirty_rect(0U, 0U, fb->width, fb->height);
    gfx_console_show_cursor();
    console_flush_unlocked();

    spin_unlock_irqrestore(&console_lock, flags);
}

framebuffer_t *console_graphics_framebuffer(void)
{
    return gfx_console.fb;
}

void console_set_output_mode(unsigned int mode)
{
    unsigned long flags;

    flags = spin_lock_irqsave(&console_lock);
    console_mode = mode & (CONSOLE_SINK_SERIAL | CONSOLE_SINK_GRAPHICS);
    if (gfx_console.fb == 0 || gfx_console.cells == 0)
    {
        console_mode &= ~CONSOLE_SINK_GRAPHICS;
    }
    if ((console_mode & CONSOLE_SINK_GRAPHICS) != 0U)
    {
        gfx_console_show_cursor();
        console_flush_unlocked();
    }
    else
    {
        gfx_console_hide_cursor();
    }
    spin_unlock_irqrestore(&console_lock, flags);
}

unsigned int console_output_mode(void)
{
    return console_mode;
}

void console_set_input_mode(unsigned int mode)
{
    input_set_mode(mode);
}

unsigned int console_input_mode(void)
{
    return input_mode();
}

void console_set_output_capture(console_output_capture_fn_t callback, void *user_data)
{
    unsigned long flags;

    flags = spin_lock_irqsave(&console_lock);
    console_capture_callback = callback;
    console_capture_user_data = user_data;
    spin_unlock_irqrestore(&console_lock, flags);
}
