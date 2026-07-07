#include "desktop/taskbar.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "lib/string.h"

#define DESKTOP_TASKBAR_HEIGHT 24U
#define DESKTOP_TASKBAR_COLOR 0xff20313cU
#define DESKTOP_TASKBAR_TEXT_COLOR 0xffffffffU
#define DESKTOP_TASKBAR_ACCENT_COLOR 0xffb9d7e3U
#define DESKTOP_TASKBAR_BUTTON_BG 0xff37505cU
#define DESKTOP_TASKBAR_BUTTON_ACTIVE_BG 0xff2a6076U
#define DESKTOP_TASKBAR_LAUNCHER_BG 0xff2e596bU
#define DESKTOP_TASKBAR_MENU_BG 0xffd9e4eaU
#define DESKTOP_TASKBAR_MENU_BORDER 0xff37505cU
#define DESKTOP_TASKBAR_MENU_TEXT 0xff102030U
#define DESKTOP_TASKBAR_PADDING 8
#define DESKTOP_TASKBAR_LAUNCHER_WIDTH 68
#define DESKTOP_TASKBAR_WINDOW_BUTTON_WIDTH 88
#define DESKTOP_TASKBAR_WINDOW_BUTTON_GAP 6
#define DESKTOP_TASKBAR_MENU_WIDTH 144
#define DESKTOP_TASKBAR_MENU_HEIGHT 72

static int desktop_taskbar_y(unsigned int screen_height)
{
    if (screen_height <= DESKTOP_TASKBAR_HEIGHT)
    {
        return 0;
    }

    return (int)(screen_height - DESKTOP_TASKBAR_HEIGHT);
}

static int desktop_taskbar_window_button_x(unsigned int button_index)
{
    return DESKTOP_TASKBAR_PADDING + DESKTOP_TASKBAR_LAUNCHER_WIDTH +
           DESKTOP_TASKBAR_WINDOW_BUTTON_GAP +
           (int)button_index * (DESKTOP_TASKBAR_WINDOW_BUTTON_WIDTH + DESKTOP_TASKBAR_WINDOW_BUTTON_GAP);
}

static void desktop_taskbar_format_uptime(unsigned long uptime_ms, char *buffer, unsigned long buffer_size)
{
    unsigned long seconds;
    unsigned long minutes;
    char tens;
    char ones;

    if (buffer == 0 || buffer_size < 9U)
    {
        return;
    }

    seconds = uptime_ms / 1000UL;
    minutes = seconds / 60UL;
    seconds %= 60UL;
    minutes %= 100UL;

    tens = (char)('0' + ((minutes / 10UL) % 10UL));
    ones = (char)('0' + (minutes % 10UL));

    buffer[0] = 'u';
    buffer[1] = 'p';
    buffer[2] = ' ';
    buffer[3] = tens;
    buffer[4] = ones;
    buffer[5] = ':';
    buffer[6] = (char)('0' + ((seconds / 10UL) % 10UL));
    buffer[7] = (char)('0' + (seconds % 10UL));
    buffer[8] = '\0';
}

unsigned int desktop_taskbar_height(void)
{
    return DESKTOP_TASKBAR_HEIGHT;
}

void desktop_taskbar_draw(framebuffer_t *fb,
                          const desktop_window_t *windows,
                          unsigned int window_count,
                          unsigned int focused_window_id,
                          unsigned long uptime_ms,
                          int launcher_open)
{
    unsigned int index;
    int bar_y;
    int button_y;
    char uptime_text[16];

    if (fb == 0 || fb->buffer == 0)
    {
        return;
    }

    bar_y = desktop_taskbar_y(fb->height);
    button_y = bar_y + 4;

    draw_fill_rect(fb, 0, bar_y, (int)fb->width, (int)DESKTOP_TASKBAR_HEIGHT, DESKTOP_TASKBAR_COLOR);

    draw_fill_rect(fb,
                   DESKTOP_TASKBAR_PADDING,
                   button_y,
                   DESKTOP_TASKBAR_LAUNCHER_WIDTH,
                   (int)DESKTOP_TASKBAR_HEIGHT - 8,
                   DESKTOP_TASKBAR_LAUNCHER_BG);
    draw_rect(fb,
              DESKTOP_TASKBAR_PADDING,
              button_y,
              DESKTOP_TASKBAR_LAUNCHER_WIDTH,
              (int)DESKTOP_TASKBAR_HEIGHT - 8,
              DESKTOP_TASKBAR_ACCENT_COLOR);
    gfx_draw_string(fb,
                    DESKTOP_TASKBAR_PADDING + 16,
                    bar_y + 8,
                    "Start",
                    DESKTOP_TASKBAR_TEXT_COLOR,
                    DESKTOP_TASKBAR_LAUNCHER_BG);

    for (index = 0; index < window_count; index++)
    {
        unsigned int bg_color;
        int button_x;

        if (!desktop_window_is_visible(&windows[index]))
        {
            continue;
        }

        button_x = desktop_taskbar_window_button_x(index);
        bg_color = windows[index].id == focused_window_id
                       ? DESKTOP_TASKBAR_BUTTON_ACTIVE_BG
                       : DESKTOP_TASKBAR_BUTTON_BG;

        draw_fill_rect(fb,
                       button_x,
                       button_y,
                       DESKTOP_TASKBAR_WINDOW_BUTTON_WIDTH,
                       (int)DESKTOP_TASKBAR_HEIGHT - 8,
                       bg_color);
        draw_rect(fb,
                  button_x,
                  button_y,
                  DESKTOP_TASKBAR_WINDOW_BUTTON_WIDTH,
                  (int)DESKTOP_TASKBAR_HEIGHT - 8,
                  DESKTOP_TASKBAR_ACCENT_COLOR);
        gfx_draw_string(fb,
                        button_x + 6,
                        bar_y + 8,
                        windows[index].title,
                        DESKTOP_TASKBAR_TEXT_COLOR,
                        bg_color);
    }

    desktop_taskbar_format_uptime(uptime_ms, uptime_text, sizeof(uptime_text));
    gfx_draw_string(fb,
                    (int)fb->width - 72,
                    bar_y + 8,
                    uptime_text,
                    DESKTOP_TASKBAR_TEXT_COLOR,
                    DESKTOP_TASKBAR_COLOR);

    if (launcher_open)
    {
        int menu_y;

        menu_y = bar_y - DESKTOP_TASKBAR_MENU_HEIGHT;
        if (menu_y < 0)
        {
            menu_y = 0;
        }

        draw_fill_rect(fb,
                       DESKTOP_TASKBAR_PADDING,
                       menu_y,
                       DESKTOP_TASKBAR_MENU_WIDTH,
                       DESKTOP_TASKBAR_MENU_HEIGHT,
                       DESKTOP_TASKBAR_MENU_BG);
        draw_rect(fb,
                  DESKTOP_TASKBAR_PADDING,
                  menu_y,
                  DESKTOP_TASKBAR_MENU_WIDTH,
                  DESKTOP_TASKBAR_MENU_HEIGHT,
                  DESKTOP_TASKBAR_MENU_BORDER);
        gfx_draw_string(fb,
                        DESKTOP_TASKBAR_PADDING + 8,
                        menu_y + 10,
                        "Launcher",
                        DESKTOP_TASKBAR_MENU_TEXT,
                        DESKTOP_TASKBAR_MENU_BG);
        gfx_draw_string(fb,
                        DESKTOP_TASKBAR_PADDING + 8,
                        menu_y + 28,
                        "desktop apps soon",
                        DESKTOP_TASKBAR_MENU_TEXT,
                        DESKTOP_TASKBAR_MENU_BG);
        gfx_draw_string(fb,
                        DESKTOP_TASKBAR_PADDING + 8,
                        menu_y + 46,
                        "press Start again",
                        DESKTOP_TASKBAR_MENU_TEXT,
                        DESKTOP_TASKBAR_MENU_BG);
    }
}

int desktop_taskbar_contains_point(unsigned int screen_height, unsigned int x, unsigned int y)
{
    (void)x;
    return y >= (unsigned int)desktop_taskbar_y(screen_height);
}

int desktop_taskbar_launcher_hit_test(unsigned int screen_height, unsigned int x, unsigned int y)
{
    int bar_y;

    bar_y = desktop_taskbar_y(screen_height);
    return x >= DESKTOP_TASKBAR_PADDING &&
           x < (DESKTOP_TASKBAR_PADDING + DESKTOP_TASKBAR_LAUNCHER_WIDTH) &&
           y >= (unsigned int)(bar_y + 4) &&
           y < (unsigned int)(bar_y + (int)DESKTOP_TASKBAR_HEIGHT - 4);
}

unsigned int desktop_taskbar_window_button_hit_test(const desktop_window_t *windows,
                                                    unsigned int window_count,
                                                    unsigned int screen_width,
                                                    unsigned int screen_height,
                                                    unsigned int x,
                                                    unsigned int y)
{
    unsigned int index;
    int bar_y;

    (void)screen_width;
    bar_y = desktop_taskbar_y(screen_height);

    for (index = 0; index < window_count; index++)
    {
        int button_x;

        if (!desktop_window_is_visible(&windows[index]))
        {
            continue;
        }

        button_x = desktop_taskbar_window_button_x(index);
        if (x >= (unsigned int)button_x &&
            x < (unsigned int)(button_x + DESKTOP_TASKBAR_WINDOW_BUTTON_WIDTH) &&
            y >= (unsigned int)(bar_y + 4) &&
            y < (unsigned int)(bar_y + (int)DESKTOP_TASKBAR_HEIGHT - 4))
        {
            return windows[index].id;
        }
    }

    return 0U;
}
