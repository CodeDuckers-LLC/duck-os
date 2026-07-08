#include "desktop/desktop_theme.h"
#include "fs/file.h"
#include "lib/string.h"

#define DESKTOP_THEME_PATH "/etc/theme"
#define DESKTOP_THEME_FILE_MAX 512U

static const desktop_theme_t desktop_theme_defaults = {
    0xff6d8fa4U,
    0xff20313cU,
    0xffd9e4eaU,
    0xff2a6076U,
    0xff546b78U,
    0xff102030U,
    0xff89aebfU
};

static desktop_theme_t desktop_theme_current;
static int desktop_theme_loaded;

static int desktop_theme_is_space(char ch);
static unsigned int desktop_theme_hex_value(char ch, int *valid_out);
static int desktop_theme_parse_color(const char *text, unsigned int *color_out);
static void desktop_theme_apply_value(const char *key, unsigned long key_length, unsigned int color);
static void desktop_theme_parse_buffer(char *buffer);

static int desktop_theme_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static unsigned int desktop_theme_hex_value(char ch, int *valid_out)
{
    if (ch >= '0' && ch <= '9')
    {
        *valid_out = 1;
        return (unsigned int)(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f')
    {
        *valid_out = 1;
        return 10U + (unsigned int)(ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F')
    {
        *valid_out = 1;
        return 10U + (unsigned int)(ch - 'A');
    }

    *valid_out = 0;
    return 0U;
}

static int desktop_theme_parse_color(const char *text, unsigned int *color_out)
{
    unsigned int color;
    unsigned int digits;
    int valid;

    if (text == 0 || color_out == 0)
    {
        return -1;
    }

    while (desktop_theme_is_space(*text))
    {
        text++;
    }
    if (*text == '#')
    {
        text++;
    }
    else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        text += 2;
    }

    color = 0U;
    digits = 0U;
    while (*text != '\0' && !desktop_theme_is_space(*text))
    {
        color = (color << 4) | desktop_theme_hex_value(*text, &valid);
        if (!valid)
        {
            return -1;
        }
        digits++;
        text++;
    }

    if (digits == 6U)
    {
        *color_out = 0xff000000U | color;
        return 0;
    }
    if (digits == 8U)
    {
        *color_out = color;
        return 0;
    }

    return -1;
}

static void desktop_theme_apply_value(const char *key, unsigned long key_length, unsigned int color)
{
    if (key_length == 10U && memcmp(key, "background", 10U) == 0)
    {
        desktop_theme_current.background = color;
    }
    else if (key_length == 7U && memcmp(key, "taskbar", 7U) == 0)
    {
        desktop_theme_current.taskbar = color;
    }
    else if (key_length == 11U && memcmp(key, "window_body", 11U) == 0)
    {
        desktop_theme_current.window_body = color;
    }
    else if (key_length == 15U && memcmp(key, "titlebar_active", 15U) == 0)
    {
        desktop_theme_current.titlebar_active = color;
    }
    else if (key_length == 17U && memcmp(key, "titlebar_inactive", 17U) == 0)
    {
        desktop_theme_current.titlebar_inactive = color;
    }
    else if (key_length == 4U && memcmp(key, "text", 4U) == 0)
    {
        desktop_theme_current.text = color;
    }
    else if (key_length == 6U && memcmp(key, "accent", 6U) == 0)
    {
        desktop_theme_current.accent = color;
    }
}

static void desktop_theme_parse_buffer(char *buffer)
{
    char *line;

    if (buffer == 0)
    {
        return;
    }

    for (line = buffer; *line != '\0'; )
    {
        char *key_start;
        char *key_end;
        char *value_start;
        char *scan;
        unsigned int color;

        while (*line == '\n' || *line == '\r')
        {
            line++;
        }
        if (*line == '\0')
        {
            break;
        }

        while (desktop_theme_is_space(*line))
        {
            line++;
        }
        if (*line == '\0')
        {
            break;
        }

        key_start = line;
        while (*line != '\0' && *line != '\n' && *line != '\r' && *line != '=' && *line != '#')
        {
            line++;
        }

        if (*line == '#')
        {
            while (*line != '\0' && *line != '\n' && *line != '\r')
            {
                line++;
            }
            continue;
        }

        key_end = line;
        while (key_end > key_start && desktop_theme_is_space(key_end[-1]))
        {
            key_end--;
        }

        if (*line != '=')
        {
            while (*line != '\0' && *line != '\n' && *line != '\r')
            {
                line++;
            }
            continue;
        }

        line++;
        while (desktop_theme_is_space(*line))
        {
            line++;
        }
        value_start = line;
        while (*line != '\0' && *line != '\n' && *line != '\r' && *line != '#')
        {
            line++;
        }

        scan = line;
        while (scan > value_start && desktop_theme_is_space(scan[-1]))
        {
            scan--;
        }
        *scan = '\0';

        if (key_end > key_start && desktop_theme_parse_color(value_start, &color) == 0)
        {
            desktop_theme_apply_value(key_start, (unsigned long)(key_end - key_start), color);
        }

        while (*line != '\0' && *line != '\n' && *line != '\r')
        {
            line++;
        }
    }
}

void desktop_theme_init(void)
{
    file_t *file;
    char buffer[DESKTOP_THEME_FILE_MAX];
    int read_size;

    desktop_theme_current = desktop_theme_defaults;
    desktop_theme_loaded = 1;

    memset(buffer, 0, sizeof(buffer));
    file = file_open(DESKTOP_THEME_PATH);
    if (file == 0)
    {
        return;
    }

    read_size = file_read(file, buffer, sizeof(buffer) - 1U);
    file_close(file);
    if (read_size <= 0)
    {
        return;
    }

    buffer[read_size] = '\0';
    desktop_theme_parse_buffer(buffer);
}

const desktop_theme_t *desktop_theme_get(void)
{
    if (!desktop_theme_loaded)
    {
        desktop_theme_init();
    }

    return &desktop_theme_current;
}

unsigned int desktop_theme_lighten(unsigned int color, unsigned int amount)
{
    unsigned int alpha;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    alpha = color & 0xff000000U;
    red = (color >> 16) & 0xffU;
    green = (color >> 8) & 0xffU;
    blue = color & 0xffU;

    red = red + amount > 0xffU ? 0xffU : red + amount;
    green = green + amount > 0xffU ? 0xffU : green + amount;
    blue = blue + amount > 0xffU ? 0xffU : blue + amount;

    return alpha | (red << 16) | (green << 8) | blue;
}

unsigned int desktop_theme_darken(unsigned int color, unsigned int amount)
{
    unsigned int alpha;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    alpha = color & 0xff000000U;
    red = (color >> 16) & 0xffU;
    green = (color >> 8) & 0xffU;
    blue = color & 0xffU;

    red = red > amount ? red - amount : 0U;
    green = green > amount ? green - amount : 0U;
    blue = blue > amount ? blue - amount : 0U;

    return alpha | (red << 16) | (green << 8) | blue;
}
