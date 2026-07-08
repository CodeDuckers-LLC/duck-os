#include "desktop/files_app.h"
#include "desktop/desktop.h"
#include "desktop/desktop_event.h"
#include "desktop/desktop_window.h"
#include "fs/vfs.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "input/input.h"
#include "lib/string.h"

#define FILES_WINDOW_WIDTH 560U
#define FILES_WINDOW_HEIGHT 260U
#define FILES_MAX_INSTANCES 8U
#define FILES_PATH_MAX 64U
#define FILES_ENTRY_NAME_MAX 32U
#define FILES_MAX_ENTRIES 32U
#define FILES_MARGIN 6
#define FILES_HEADER_HEIGHT 16
#define FILES_ROW_HEIGHT 12
#define FILES_BG_COLOR 0xffedf4f8U
#define FILES_HEADER_BG 0xffd7e5eeU
#define FILES_TEXT_COLOR 0xff102030U
#define FILES_MUTED_COLOR 0xff5f7280U
#define FILES_BORDER_COLOR 0xff7890a1U
#define FILES_SELECTION_BG 0xffbfd9e7U

typedef struct files_entry
{
    char name[FILES_ENTRY_NAME_MAX];
    unsigned int size;
    unsigned int flags;
} files_entry_t;

typedef struct files_app_state
{
    char path[FILES_PATH_MAX];
    char status[FILES_PATH_MAX];
    files_entry_t entries[FILES_MAX_ENTRIES];
    unsigned int entry_count;
    unsigned int selected_index;
    unsigned int scroll_offset;
    int active;
} files_app_state_t;

static files_app_state_t files_states[FILES_MAX_INSTANCES];

static files_app_state_t *files_app_alloc_state(void);
static void files_app_set_status(files_app_state_t *state, const char *status);
static int files_app_collect_entry(const vfs_file_info_t *entry, void *context);
static void files_app_refresh(files_app_state_t *state);
static void files_app_set_path(files_app_state_t *state, const char *path);
static int files_app_build_child_path(const files_app_state_t *state,
                                      const files_entry_t *entry,
                                      char *path_out,
                                      unsigned long path_out_size);
static void files_app_go_parent(files_app_state_t *state);
static void files_app_open_selected(files_app_state_t *state);
static void files_app_ensure_selection_visible(files_app_state_t *state, unsigned int visible_rows);
static int files_app_row_hit_test(const desktop_window_t *window,
                                  const files_app_state_t *state,
                                  unsigned int x,
                                  unsigned int y,
                                  unsigned int *entry_index_out);

static files_app_state_t *files_app_alloc_state(void)
{
    unsigned int index;

    for (index = 0; index < FILES_MAX_INSTANCES; index++)
    {
        if (!files_states[index].active)
        {
            memset(&files_states[index], 0, sizeof(files_states[index]));
            files_states[index].active = 1;
            strlcpy(files_states[index].path, "/", sizeof(files_states[index].path));
            files_app_set_status(&files_states[index], "ready");
            return &files_states[index];
        }
    }

    return 0;
}

static void files_app_set_status(files_app_state_t *state, const char *status)
{
    if (state == 0)
    {
        return;
    }

    if (status == 0)
    {
        state->status[0] = '\0';
        return;
    }

    strlcpy(state->status, status, sizeof(state->status));
}

static int files_app_collect_entry(const vfs_file_info_t *entry, void *context)
{
    files_app_state_t *state;
    files_entry_t *slot;

    state = (files_app_state_t *)context;
    if (state == 0 || entry == 0)
    {
        return -1;
    }

    if (state->entry_count >= FILES_MAX_ENTRIES)
    {
        return 1;
    }

    slot = &state->entries[state->entry_count++];
    memset(slot, 0, sizeof(*slot));
    strlcpy(slot->name, entry->name, sizeof(slot->name));
    slot->size = entry->size;
    slot->flags = entry->flags;
    return 0;
}

static void files_app_refresh(files_app_state_t *state)
{
    if (state == 0)
    {
        return;
    }

    memset(state->entries, 0, sizeof(state->entries));
    state->entry_count = 0U;
    state->selected_index = 0U;
    state->scroll_offset = 0U;

    if (vfs_list_entries(state->path, files_app_collect_entry, state) != 0)
    {
        files_app_set_status(state, "list failed");
        return;
    }

    if (state->entry_count == 0U)
    {
        files_app_set_status(state, "empty directory");
    }
    else
    {
        files_app_set_status(state, "read only");
    }
}

static void files_app_set_path(files_app_state_t *state, const char *path)
{
    if (state == 0 || path == 0 || *path == '\0')
    {
        return;
    }

    strlcpy(state->path, path, sizeof(state->path));
    files_app_refresh(state);
}

static int files_app_build_child_path(const files_app_state_t *state,
                                      const files_entry_t *entry,
                                      char *path_out,
                                      unsigned long path_out_size)
{
    unsigned long used;

    if (state == 0 || entry == 0 || path_out == 0 || path_out_size == 0U)
    {
        return -1;
    }

    memset(path_out, 0, path_out_size);
    if (strcmp(state->path, "/") == 0)
    {
        strlcpy(path_out, "/", path_out_size);
        used = strlen(path_out);
    }
    else
    {
        strlcpy(path_out, state->path, path_out_size);
        used = strlen(path_out);
        if (used + 1U >= path_out_size)
        {
            return -1;
        }
        path_out[used++] = '/';
        path_out[used] = '\0';
    }

    strlcpy(path_out + used, entry->name, path_out_size - used);
    return 0;
}

static void files_app_go_parent(files_app_state_t *state)
{
    char *scan;

    if (state == 0 || strcmp(state->path, "/") == 0)
    {
        return;
    }

    scan = state->path + strlen(state->path);
    while (scan > state->path && scan[-1] != '/')
    {
        scan--;
    }

    if (scan <= state->path + 1)
    {
        strlcpy(state->path, "/", sizeof(state->path));
    }
    else
    {
        scan[-1] = '\0';
    }

    files_app_refresh(state);
}

static void files_app_open_selected(files_app_state_t *state)
{
    char child_path[FILES_PATH_MAX];
    const files_entry_t *entry;

    if (state == 0 || state->entry_count == 0U || state->selected_index >= state->entry_count)
    {
        return;
    }

    entry = &state->entries[state->selected_index];
    if (files_app_build_child_path(state, entry, child_path, sizeof(child_path)) != 0)
    {
        files_app_set_status(state, "path too long");
        return;
    }

    if ((entry->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        files_app_set_path(state, child_path);
        return;
    }

    if (desktop_open_file(child_path) != 0)
    {
        files_app_set_status(state, "open failed");
        return;
    }

    files_app_set_status(state, "opened file");
}

static void files_app_ensure_selection_visible(files_app_state_t *state, unsigned int visible_rows)
{
    if (state == 0 || visible_rows == 0U)
    {
        return;
    }

    if (state->selected_index < state->scroll_offset)
    {
        state->scroll_offset = state->selected_index;
    }
    else if (state->selected_index >= state->scroll_offset + visible_rows)
    {
        state->scroll_offset = state->selected_index - visible_rows + 1U;
    }
}

static int files_app_row_hit_test(const desktop_window_t *window,
                                  const files_app_state_t *state,
                                  unsigned int x,
                                  unsigned int y,
                                  unsigned int *entry_index_out)
{
    int content_x;
    int content_y;
    unsigned int content_width;
    unsigned int content_height;
    unsigned int row_area_top;
    unsigned int row;
    unsigned int visible_rows;
    unsigned int index;

    if (window == 0 || state == 0)
    {
        return 0;
    }

    content_x = desktop_window_content_x(window);
    content_y = desktop_window_content_y(window);
    content_width = desktop_window_content_width(window);
    content_height = desktop_window_content_height(window);
    row_area_top = (unsigned int)(content_y + FILES_HEADER_HEIGHT + 8);
    visible_rows = (content_height > FILES_HEADER_HEIGHT + 24U)
                       ? (content_height - FILES_HEADER_HEIGHT - 24U) / FILES_ROW_HEIGHT
                       : 0U;

    if (x < (unsigned int)(content_x + FILES_MARGIN) ||
        x >= (unsigned int)(content_x + (int)content_width - FILES_MARGIN) ||
        y < row_area_top)
    {
        return 0;
    }

    row = (y - row_area_top) / FILES_ROW_HEIGHT;
    if (row >= visible_rows)
    {
        return 0;
    }

    index = state->scroll_offset + row;
    if (index >= state->entry_count)
    {
        return 0;
    }

    if (entry_index_out != 0)
    {
        *entry_index_out = index;
    }
    return 1;
}

void files_app_render(desktop_app_instance_t *instance,
                      const desktop_window_t *window,
                      framebuffer_t *fb)
{
    files_app_state_t *state;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    unsigned int visible_rows;
    unsigned int row;
    char line[FILES_ENTRY_NAME_MAX + 8U];

    (void)instance;

    if (window == 0)
    {
        return;
    }

    state = (files_app_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    x = desktop_window_content_x(window);
    y = desktop_window_content_y(window);
    width = desktop_window_content_width(window);
    height = desktop_window_content_height(window);

    draw_fill_rect(fb, x, y, (int)width, (int)height, FILES_BG_COLOR);
    draw_rect(fb, x, y, (int)width, (int)height, FILES_BORDER_COLOR);
    draw_fill_rect(fb, x, y, (int)width, FILES_HEADER_HEIGHT, FILES_HEADER_BG);

    gfx_draw_string(fb, x + FILES_MARGIN, y + 4, state->path, FILES_TEXT_COLOR, FILES_HEADER_BG);
    gfx_draw_string(fb, x + FILES_MARGIN, y + FILES_HEADER_HEIGHT + 4, "Enter open  Backspace up", FILES_MUTED_COLOR, FILES_BG_COLOR);

    visible_rows = (height > FILES_HEADER_HEIGHT + 24U)
                       ? (height - FILES_HEADER_HEIGHT - 24U) / FILES_ROW_HEIGHT
                       : 0U;
    files_app_ensure_selection_visible(state, visible_rows);

    for (row = 0; row < visible_rows; row++)
    {
        unsigned int index;
        unsigned int bg_color;
        int row_y;

        index = state->scroll_offset + row;
        if (index >= state->entry_count)
        {
            break;
        }

        bg_color = index == state->selected_index ? FILES_SELECTION_BG : FILES_BG_COLOR;
        row_y = y + FILES_HEADER_HEIGHT + 18 + (int)(row * FILES_ROW_HEIGHT);
        draw_fill_rect(fb, x + 2, row_y - 1, (int)width - 4, FILES_ROW_HEIGHT - 1, bg_color);

        memset(line, 0, sizeof(line));
        if ((state->entries[index].flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
        {
            strlcpy(line, "[D] ", sizeof(line));
        }
        else
        {
            strlcpy(line, "[F] ", sizeof(line));
        }
        strlcpy(line + strlen(line),
                state->entries[index].name,
                sizeof(line) - strlen(line));

        gfx_draw_string(fb, x + FILES_MARGIN, row_y + 2, line, FILES_TEXT_COLOR, bg_color);
    }

    gfx_draw_string(fb,
                    x + FILES_MARGIN,
                    y + (int)height - 12,
                    state->status,
                    FILES_MUTED_COLOR,
                    FILES_BG_COLOR);
}

void files_app_event(desktop_app_instance_t *instance,
                     const desktop_window_t *window,
                     const desktop_event_t *event)
{
    files_app_state_t *state;
    unsigned int visible_rows;

    (void)instance;

    if (window == 0 || event == 0)
    {
        return;
    }

    state = (files_app_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    visible_rows = (desktop_window_content_height(window) > FILES_HEADER_HEIGHT + 24U)
                       ? (desktop_window_content_height(window) - FILES_HEADER_HEIGHT - 24U) / FILES_ROW_HEIGHT
                       : 0U;

    if (event->type == DESKTOP_EVENT_WINDOW_CLOSE)
    {
        return;
    }

    if (event->type == DESKTOP_EVENT_BUTTON_DOWN)
    {
        unsigned int hit_index;

        if (files_app_row_hit_test(window, state, event->cursor_x, event->cursor_y, &hit_index))
        {
            state->selected_index = hit_index;
            files_app_ensure_selection_visible(state, visible_rows);
        }
        return;
    }

    if (event->type != DESKTOP_EVENT_KEY ||
        (event->input.pressed != INPUT_KEY_PRESS && event->input.pressed != INPUT_KEY_REPEAT))
    {
        return;
    }

    if (event->input.data.keycode == INPUT_KEY_UP)
    {
        if (state->selected_index > 0U)
        {
            state->selected_index--;
            files_app_ensure_selection_visible(state, visible_rows);
        }
        return;
    }

    if (event->input.data.keycode == INPUT_KEY_DOWN)
    {
        if (state->selected_index + 1U < state->entry_count)
        {
            state->selected_index++;
            files_app_ensure_selection_visible(state, visible_rows);
        }
        return;
    }

    if (event->input.data.keycode == INPUT_KEY_ENTER)
    {
        files_app_open_selected(state);
        return;
    }

    if (event->input.data.keycode == INPUT_KEY_BACKSPACE)
    {
        files_app_go_parent(state);
        return;
    }
}

int files_app_start(desktop_app_instance_t *instance)
{
    files_app_state_t *state;

    if (instance == 0)
    {
        return -1;
    }

    state = files_app_alloc_state();
    if (state == 0)
    {
        return -1;
    }

    if (instance->argument[0] != '\0')
    {
        files_app_set_path(state, instance->argument);
    }
    else
    {
        files_app_refresh(state);
    }

    instance->user_data = state;
    return desktop_open_app_instance_window(instance,
                                            "Files",
                                            FILES_WINDOW_WIDTH,
                                            FILES_WINDOW_HEIGHT,
                                            state);
}

void files_app_close(desktop_app_instance_t *instance)
{
    files_app_state_t *state;

    if (instance == 0)
    {
        return;
    }

    state = (files_app_state_t *)instance->user_data;
    if (state != 0)
    {
        memset(state, 0, sizeof(*state));
    }
}
