#include "drivers/virtio_gpu.h"
#include "fs/vfs.h"
#include "gfx/font.h"
#include "gui/app_file_browser.h"
#include "gui/gui.h"
#include "lib/string.h"

#define APP_FILE_BROWSER_MAX_ENTRIES 16U
#define APP_FILE_BROWSER_NAME_MAX 32U
#define APP_FILE_BROWSER_PATH_MAX 64U
#define APP_FILE_BROWSER_PREVIEW_MAX 384U

typedef struct file_browser_entry
{
    char name[APP_FILE_BROWSER_NAME_MAX];
    char path[APP_FILE_BROWSER_PATH_MAX];
    unsigned int size;
    unsigned int flags;
} file_browser_entry_t;

typedef struct file_browser_state
{
    unsigned int window_id;
    unsigned int selected_index;
    unsigned int entry_count;
    char selected_path[APP_FILE_BROWSER_PATH_MAX];
    char preview[APP_FILE_BROWSER_PREVIEW_MAX];
    file_browser_entry_t entries[APP_FILE_BROWSER_MAX_ENTRIES];
} file_browser_state_t;

static file_browser_state_t file_browser_state;

static int app_file_browser_collect_entry(const vfs_file_info_t *entry, void *context)
{
    file_browser_state_t *state;
    file_browser_entry_t *slot;

    state = (file_browser_state_t *)context;
    if (state == 0 || entry == 0)
    {
        return -1;
    }

    if (state->entry_count >= APP_FILE_BROWSER_MAX_ENTRIES)
    {
        return 1;
    }

    slot = &state->entries[state->entry_count];
    memset(slot, 0, sizeof(*slot));
    strlcpy(slot->name, entry->name, sizeof(slot->name));
    slot->path[0] = '/';
    strlcpy(slot->path + 1, entry->name, sizeof(slot->path) - 1U);
    slot->size = entry->size;
    slot->flags = entry->flags;
    state->entry_count++;
    return 0;
}

static void app_file_browser_load_preview(file_browser_state_t *state)
{
    const vfs_file_info_t *info;
    unsigned int preview_size;
    int read_size;
    unsigned int i;

    if (state == 0)
    {
        return;
    }

    memset(state->preview, 0, sizeof(state->preview));
    if (state->entry_count == 0U || state->selected_index >= state->entry_count)
    {
        strlcpy(state->preview, "(no files)", sizeof(state->preview));
        state->selected_path[0] = '\0';
        return;
    }

    strlcpy(state->selected_path,
            state->entries[state->selected_index].path,
            sizeof(state->selected_path));

    info = vfs_stat(state->selected_path);
    if (info == 0)
    {
        strlcpy(state->preview, "(stat failed)", sizeof(state->preview));
        return;
    }

    if ((info->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        strlcpy(state->preview, "(directory)", sizeof(state->preview));
        return;
    }

    preview_size = sizeof(state->preview) - 1U;
    read_size = vfs_read_file(state->selected_path, state->preview, preview_size);
    if (read_size < 0)
    {
        strlcpy(state->preview, "(read failed)", sizeof(state->preview));
        return;
    }

    state->preview[read_size] = '\0';
    for (i = 0; i < (unsigned int)read_size; i++)
    {
        unsigned char ch;

        ch = (unsigned char)state->preview[i];
        if ((ch < 32U || ch > 126U) && ch != '\n' && ch != '\r' && ch != '\t')
        {
            state->preview[i] = '.';
        }
    }
}

static void app_file_browser_draw(window_t *window, framebuffer_t *fb)
{
    int content_x;
    int content_y;
    unsigned int content_width;
    unsigned int content_height;
    unsigned int left_width;
    unsigned int right_width;
    unsigned int i;
    int line_y;

    content_x = gui_window_content_x(window);
    content_y = gui_window_content_y(window);
    content_width = gui_window_content_width(window);
    content_height = gui_window_content_height(window);
    if (content_width < 48U || content_height < 48U)
    {
        return;
    }

    left_width = content_width / 3U;
    if (left_width < 120U)
    {
        left_width = 120U;
    }
    if (left_width + 16U >= content_width)
    {
        left_width = content_width / 2U;
    }
    right_width = content_width - left_width - 8U;

    gui_draw_panel(fb, content_x, content_y, left_width, content_height, 0xffeef4f8U, 0xff7b8fa1U);
    gui_draw_panel(fb, content_x + (int)left_width + 8, content_y, right_width, content_height, 0xfff9f3eaU, 0xff9d7f67U);

    gui_draw_label(fb, content_x + 8, content_y + 8, "Files in /", 0xff102030U, 0xffeef4f8U);
    line_y = content_y + 24;
    for (i = 0; i < file_browser_state.entry_count && line_y + GFX_FONT_HEIGHT < content_y + (int)content_height; i++)
    {
        unsigned int bg_color;
        char line[48];

        bg_color = (i == file_browser_state.selected_index) ? 0xffcfe2f3U : 0xffeef4f8U;
        gui_draw_panel(fb, content_x + 6, line_y - 2, left_width - 12U, 12U, bg_color, bg_color);
        memset(line, 0, sizeof(line));
        strlcpy(line, file_browser_state.entries[i].name, sizeof(line));
        if ((file_browser_state.entries[i].flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
        {
            unsigned long len;

            len = strlen(line);
            if (len + 1U < sizeof(line))
            {
                line[len] = '/';
                line[len + 1U] = '\0';
            }
        }
        gui_draw_label(fb, content_x + 10, line_y, line, 0xff102030U, bg_color);
        line_y += 12;
    }

    gui_draw_label(fb, content_x + (int)left_width + 16, content_y + 8, "Preview", 0xff3b2f2fU, 0xfff9f3eaU);
    if (file_browser_state.selected_path[0] != '\0')
    {
        gui_draw_label(fb,
                       content_x + (int)left_width + 16,
                       content_y + 20,
                       file_browser_state.selected_path,
                       0xff7a5c44U,
                       0xfff9f3eaU);
    }

    line_y = content_y + 36;
    for (i = 0; file_browser_state.preview[i] != '\0' && line_y + GFX_FONT_HEIGHT < content_y + (int)content_height; )
    {
        char line[40];
        unsigned int line_len;

        line_len = 0U;
        while (file_browser_state.preview[i] != '\0' &&
               file_browser_state.preview[i] != '\n' &&
               line_len + 1U < sizeof(line))
        {
            line[line_len++] = file_browser_state.preview[i++];
        }
        if (file_browser_state.preview[i] == '\n')
        {
            i++;
        }
        line[line_len] = '\0';
        gui_draw_label(fb, content_x + (int)left_width + 16, line_y, line, 0xff3b2f2fU, 0xfff9f3eaU);
        line_y += 10;
    }
}

int app_file_browser_open(unsigned int selected_index)
{
    framebuffer_t *fb;
    window_t *window;

    fb = gui_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        return -1;
    }

    if (file_browser_state.window_id != 0U)
    {
        gui_destroy_window(file_browser_state.window_id);
        memset(&file_browser_state, 0, sizeof(file_browser_state));
    }

    if (vfs_list_entries("/", app_file_browser_collect_entry, &file_browser_state) != 0)
    {
        return -1;
    }

    if (file_browser_state.entry_count == 0U || selected_index >= file_browser_state.entry_count)
    {
        file_browser_state.selected_index = 0U;
    }
    else
    {
        file_browser_state.selected_index = selected_index;
    }

    app_file_browser_load_preview(&file_browser_state);

    window = gui_create_window(48, 56, 760U, 320U, "File Browser", app_file_browser_draw);
    if (window == 0)
    {
        memset(&file_browser_state, 0, sizeof(file_browser_state));
        return -1;
    }

    file_browser_state.window_id = window->id;
    gui_draw_all();
    return 0;
}
