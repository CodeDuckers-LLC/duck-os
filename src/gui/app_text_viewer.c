#include "drivers/virtio_gpu.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "gfx/font.h"
#include "gui/app_text_viewer.h"
#include "gui/gui.h"
#include "lib/string.h"

#define APP_TEXT_VIEWER_PATH_MAX 64U
#define APP_TEXT_VIEWER_BUFFER_MAX 1024U
#define APP_TEXT_VIEWER_TITLE_MAX 32U

typedef struct text_viewer_state
{
    unsigned int window_id;
    char path[APP_TEXT_VIEWER_PATH_MAX];
    char title[APP_TEXT_VIEWER_TITLE_MAX];
    char text[APP_TEXT_VIEWER_BUFFER_MAX];
} text_viewer_state_t;

static text_viewer_state_t text_viewer_state;

static void app_text_viewer_set_message(const char *message)
{
    strlcpy(text_viewer_state.text, message, sizeof(text_viewer_state.text));
}

static void app_text_viewer_sanitize_text(unsigned int length)
{
    unsigned int i;

    for (i = 0; i < length; i++)
    {
        unsigned char ch;

        ch = (unsigned char)text_viewer_state.text[i];
        if ((ch < 32U || ch > 126U) && ch != '\n' && ch != '\r' && ch != '\t')
        {
            text_viewer_state.text[i] = '.';
        }
    }
}

static void app_text_viewer_draw(window_t *window, framebuffer_t *fb)
{
    int content_x;
    int content_y;
    unsigned int content_width;
    unsigned int content_height;
    unsigned int max_cols;
    int line_y;
    unsigned int i;

    content_x = gui_window_content_x(window);
    content_y = gui_window_content_y(window);
    content_width = gui_window_content_width(window);
    content_height = gui_window_content_height(window);
    if (content_width < 16U || content_height < 16U)
    {
        return;
    }

    gui_draw_panel(fb, content_x, content_y, content_width, content_height, 0xfff4efe6U, 0xff9d7f67U);
    gui_draw_label(fb, content_x + 8, content_y + 8, text_viewer_state.path, 0xff7a5c44U, 0xfff4efe6U);

    max_cols = (content_width - 16U) / (unsigned int)GFX_FONT_WIDTH;
    line_y = content_y + 24;
    for (i = 0; text_viewer_state.text[i] != '\0' && line_y + GFX_FONT_HEIGHT <= content_y + (int)content_height; )
    {
        char line[80];
        unsigned int line_len;

        line_len = 0U;
        while (text_viewer_state.text[i] != '\0' &&
               text_viewer_state.text[i] != '\n' &&
               text_viewer_state.text[i] != '\r')
        {
            char ch;

            ch = text_viewer_state.text[i++];
            if (ch == '\t')
            {
                ch = ' ';
            }

            if (line_len + 1U < sizeof(line) && line_len < max_cols)
            {
                line[line_len++] = ch;
            }
        }

        while (text_viewer_state.text[i] == '\r' || text_viewer_state.text[i] == '\n')
        {
            if (text_viewer_state.text[i] == '\r' && text_viewer_state.text[i + 1U] == '\n')
            {
                i++;
            }
            i++;
            break;
        }

        line[line_len] = '\0';
        gui_draw_label(fb, content_x + 8, line_y, line, 0xff3b2f2fU, 0xfff4efe6U);
        line_y += GFX_FONT_HEIGHT + 2;
    }
}

int app_text_viewer_open(const char *path)
{
    const vfs_file_info_t *info;
    framebuffer_t *fb;
    file_t *file;
    window_t *window;
    unsigned int total_read;
    int read_size;

    if (path == 0 || *path == '\0')
    {
        return -1;
    }

    fb = gui_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        return -1;
    }

    info = vfs_stat(path);
    if (info == 0 || (info->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        return -1;
    }

    if (text_viewer_state.window_id != 0U)
    {
        gui_destroy_window(text_viewer_state.window_id);
        memset(&text_viewer_state, 0, sizeof(text_viewer_state));
    }

    strlcpy(text_viewer_state.path, path, sizeof(text_viewer_state.path));
    strlcpy(text_viewer_state.title, info->name, sizeof(text_viewer_state.title));

    file = file_open(path);
    if (file == 0)
    {
        app_text_viewer_set_message("(open failed)");
    }
    else
    {
        total_read = 0U;
        while (total_read + 1U < sizeof(text_viewer_state.text))
        {
            read_size = file_read(file,
                                  text_viewer_state.text + total_read,
                                  sizeof(text_viewer_state.text) - total_read - 1U);
            if (read_size < 0)
            {
                app_text_viewer_set_message("(read failed)");
                total_read = 0U;
                break;
            }
            if (read_size == 0)
            {
                break;
            }
            total_read += (unsigned int)read_size;
        }
        text_viewer_state.text[total_read] = '\0';
        file_close(file);

        if (total_read == 0U && text_viewer_state.text[0] == '\0')
        {
            app_text_viewer_set_message("(empty file)");
        }
        else
        {
            app_text_viewer_sanitize_text(total_read);
        }
    }

    window = gui_create_window(72, 64, 760U, 360U, "Text Viewer", app_text_viewer_draw);
    if (window == 0)
    {
        memset(&text_viewer_state, 0, sizeof(text_viewer_state));
        return -1;
    }

    text_viewer_state.window_id = window->id;
    gui_draw_all();
    return 0;
}
