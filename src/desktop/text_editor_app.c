#include "desktop/text_editor_app.h"
#include "desktop/desktop.h"
#include "desktop/desktop_event.h"
#include "desktop/desktop_window.h"
#include "fs/file.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "input/input.h"
#include "lib/string.h"

#define EDITOR_WINDOW_WIDTH 620U
#define EDITOR_WINDOW_HEIGHT 300U
#define EDITOR_TEXT_MAX 4096U
#define EDITOR_STATUS_MAX 64U
#define EDITOR_PATH_MAX 64U
#define EDITOR_BG_COLOR 0xfff7f1e5U
#define EDITOR_TEXT_COLOR 0xff1d2530U
#define EDITOR_MUTED_COLOR 0xff5d6c79U
#define EDITOR_BORDER_COLOR 0xff8c9aa6U
#define EDITOR_CURSOR_COLOR 0xff1e7ea6U
#define EDITOR_BUTTON_BG 0xffd8e6ecU
#define EDITOR_BUTTON_FG 0xff16313dU
#define EDITOR_LINE_SPACING 2
#define EDITOR_MARGIN 6
#define EDITOR_HEADER_HEIGHT 18
#define EDITOR_STATUS_HEIGHT 14
#define EDITOR_MAX_INSTANCES 8U

typedef struct text_editor_state
{
    char text[EDITOR_TEXT_MAX];
    unsigned int length;
    unsigned int cursor;
    char status[EDITOR_STATUS_MAX];
    char path[EDITOR_PATH_MAX];
    int active;
} text_editor_state_t;

static text_editor_state_t editor_states[EDITOR_MAX_INSTANCES];

static void text_editor_set_status(text_editor_state_t *state, const char *message);
static int text_editor_insert_char(text_editor_state_t *state, char ch);
static void text_editor_backspace(text_editor_state_t *state);
static unsigned int text_editor_cursor_prev_line(const text_editor_state_t *state, unsigned int columns);
static unsigned int text_editor_cursor_next_line(const text_editor_state_t *state, unsigned int columns);
static unsigned int text_editor_find_line_start(const text_editor_state_t *state, unsigned int index, unsigned int columns);
static unsigned int text_editor_find_prev_line_start(const text_editor_state_t *state, unsigned int line_start, unsigned int columns);
static unsigned int text_editor_find_next_line_start(const text_editor_state_t *state, unsigned int line_start, unsigned int columns);
static unsigned int text_editor_line_length(const text_editor_state_t *state, unsigned int line_start, unsigned int columns);
static void text_editor_load_file(text_editor_state_t *state, const char *path);
static void text_editor_reset(text_editor_state_t *state);
static int text_editor_save_button_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y);
static text_editor_state_t *text_editor_alloc_state(void);

static text_editor_state_t *text_editor_alloc_state(void)
{
    unsigned int index;

    for (index = 0; index < EDITOR_MAX_INSTANCES; index++)
    {
        if (!editor_states[index].active)
        {
            memset(&editor_states[index], 0, sizeof(editor_states[index]));
            editor_states[index].active = 1;
            text_editor_set_status(&editor_states[index], "ready");
            return &editor_states[index];
        }
    }

    return 0;
}

static void text_editor_set_status(text_editor_state_t *state, const char *message)
{
    if (state == 0)
    {
        return;
    }

    if (message == 0)
    {
        state->status[0] = '\0';
        return;
    }

    strlcpy(state->status, message, sizeof(state->status));
}

static void text_editor_reset(text_editor_state_t *state)
{
    int active;

    if (state == 0)
    {
        return;
    }

    active = state->active;
    memset(state, 0, sizeof(*state));
    state->active = active;
    text_editor_set_status(state, "ready");
}

static int text_editor_insert_char(text_editor_state_t *state, char ch)
{
    unsigned int index;

    if (state == 0 || state->length + 1U >= EDITOR_TEXT_MAX)
    {
        text_editor_set_status(state, "buffer full");
        return -1;
    }

    for (index = state->length; index > state->cursor; index--)
    {
        state->text[index] = state->text[index - 1U];
    }

    state->text[state->cursor] = ch;
    state->length++;
    state->cursor++;
    state->text[state->length] = '\0';
    return 0;
}

static void text_editor_backspace(text_editor_state_t *state)
{
    unsigned int index;

    if (state == 0 || state->cursor == 0U || state->length == 0U)
    {
        return;
    }

    for (index = state->cursor - 1U; index < state->length - 1U; index++)
    {
        state->text[index] = state->text[index + 1U];
    }

    state->cursor--;
    state->length--;
    state->text[state->length] = '\0';
}

static unsigned int text_editor_find_line_start(const text_editor_state_t *state, unsigned int index, unsigned int columns)
{
    unsigned int scan;
    unsigned int count;
    unsigned int start;

    if (columns == 0U)
    {
        return 0U;
    }

    if (index > state->length)
    {
        index = state->length;
    }

    scan = 0U;
    count = 0U;
    start = 0U;
    while (scan < index)
    {
        if (count == 0U)
        {
            start = scan;
        }

        if (state->text[scan] == '\n')
        {
            scan++;
            count = 0U;
            start = scan;
            continue;
        }

        scan++;
        count++;
        if (count >= columns)
        {
            count = 0U;
        }
    }

    return start;
}

static unsigned int text_editor_find_prev_line_start(const text_editor_state_t *state, unsigned int line_start, unsigned int columns)
{
    if (line_start == 0U)
    {
        return 0U;
    }

    return text_editor_find_line_start(state, line_start - 1U, columns);
}

static unsigned int text_editor_find_next_line_start(const text_editor_state_t *state, unsigned int line_start, unsigned int columns)
{
    unsigned int index;
    unsigned int count;

    if (columns == 0U)
    {
        return line_start;
    }

    index = line_start;
    count = 0U;
    while (index < state->length)
    {
        if (state->text[index] == '\n')
        {
            return index + 1U;
        }

        index++;
        count++;
        if (count >= columns)
        {
            return index;
        }
    }

    return state->length;
}

static unsigned int text_editor_line_length(const text_editor_state_t *state, unsigned int line_start, unsigned int columns)
{
    unsigned int index;
    unsigned int count;

    index = line_start;
    count = 0U;
    while (index < state->length && count < columns)
    {
        if (state->text[index] == '\n')
        {
            break;
        }
        index++;
        count++;
    }

    return count;
}

static unsigned int text_editor_cursor_prev_line(const text_editor_state_t *state, unsigned int columns)
{
    unsigned int line_start;
    unsigned int prev_start;
    unsigned int column;
    unsigned int prev_length;

    line_start = text_editor_find_line_start(state, state->cursor, columns);
    if (line_start == 0U)
    {
        return state->cursor;
    }

    prev_start = text_editor_find_prev_line_start(state, line_start, columns);
    column = state->cursor - line_start;
    prev_length = text_editor_line_length(state, prev_start, columns);
    if (column > prev_length)
    {
        column = prev_length;
    }

    return prev_start + column;
}

static unsigned int text_editor_cursor_next_line(const text_editor_state_t *state, unsigned int columns)
{
    unsigned int line_start;
    unsigned int next_start;
    unsigned int column;
    unsigned int next_length;

    line_start = text_editor_find_line_start(state, state->cursor, columns);
    next_start = text_editor_find_next_line_start(state, line_start, columns);
    if (next_start >= state->length && next_start == line_start)
    {
        return state->cursor;
    }

    column = state->cursor - line_start;
    next_length = text_editor_line_length(state, next_start, columns);
    if (column > next_length)
    {
        column = next_length;
    }

    return next_start + column;
}

static void text_editor_load_file(text_editor_state_t *state, const char *path)
{
    file_t *file;
    int read_size;

    if (state == 0)
    {
        return;
    }

    text_editor_reset(state);

    if (path == 0 || *path == '\0')
    {
        text_editor_set_status(state, "new file");
        return;
    }

    strlcpy(state->path, path, sizeof(state->path));
    file = file_open(path);
    if (file == 0)
    {
        text_editor_set_status(state, "open failed");
        (void)desktop_show_alert("File Open Error", "Editor could not open file");
        return;
    }

    read_size = file_read(file, state->text, EDITOR_TEXT_MAX - 1U);
    if (read_size < 0)
    {
        file_close(file);
        state->text[0] = '\0';
        state->length = 0U;
        text_editor_set_status(state, "read failed");
        (void)desktop_show_alert("File Open Error", "Editor could not read file");
        return;
    }

    file_close(file);
    state->length = (unsigned int)read_size;
    state->text[state->length] = '\0';
    state->cursor = state->length;
    text_editor_set_status(state, "file loaded");
}

static int text_editor_save_button_hit_test(const desktop_window_t *window, unsigned int x, unsigned int y)
{
    int content_x;
    int content_y;
    unsigned int content_width;
    int button_x;
    int button_y;

    if (window == 0)
    {
        return 0;
    }

    content_x = desktop_window_content_x(window);
    content_y = desktop_window_content_y(window);
    content_width = desktop_window_content_width(window);
    button_x = content_x + (int)content_width - 56;
    button_y = content_y + 2;

    return x >= (unsigned int)button_x &&
           x < (unsigned int)(button_x + 48) &&
           y >= (unsigned int)button_y &&
           y < (unsigned int)(button_y + 14);
}

void text_editor_app_render(desktop_app_instance_t *instance,
                            const desktop_window_t *window,
                            framebuffer_t *fb)
{
    text_editor_state_t *state;
    unsigned int content_width;
    unsigned int content_height;
    unsigned int text_top;
    unsigned int text_height;
    unsigned int columns;
    unsigned int rows;
    unsigned int cursor_line_start;
    unsigned int view_start;
    unsigned int walk;
    unsigned int line_count;
    int x;
    int y;
    char header_text[EDITOR_PATH_MAX + 8U];

    (void)instance;

    if (window == 0)
    {
        return;
    }

    state = (text_editor_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    x = desktop_window_content_x(window);
    y = desktop_window_content_y(window);
    content_width = desktop_window_content_width(window);
    content_height = desktop_window_content_height(window);
    text_top = (unsigned int)(y + EDITOR_HEADER_HEIGHT);
    text_height = content_height - EDITOR_HEADER_HEIGHT - EDITOR_STATUS_HEIGHT;

    draw_fill_rect(fb, x, y, (int)content_width, (int)content_height, EDITOR_BG_COLOR);
    draw_rect(fb, x, y, (int)content_width, (int)content_height, EDITOR_BORDER_COLOR);

    draw_fill_rect(fb, x, y, (int)content_width, EDITOR_HEADER_HEIGHT, 0xffe5dcc8U);
    draw_fill_rect(fb, x, (int)(y + content_height - EDITOR_STATUS_HEIGHT), (int)content_width, EDITOR_STATUS_HEIGHT, 0xffece5d8U);

    draw_fill_rect(fb, x + (int)content_width - 56, y + 2, 48, 14, EDITOR_BUTTON_BG);
    draw_rect(fb, x + (int)content_width - 56, y + 2, 48, 14, EDITOR_BORDER_COLOR);
    gfx_draw_string(fb, x + (int)content_width - 45, y + 5, "Save", EDITOR_BUTTON_FG, EDITOR_BUTTON_BG);

    memset(header_text, 0, sizeof(header_text));
    if (state->path[0] != '\0')
    {
        strlcpy(header_text, state->path, sizeof(header_text));
    }
    else
    {
        strlcpy(header_text, "(untitled)", sizeof(header_text));
    }
    gfx_draw_string(fb, x + EDITOR_MARGIN, y + 5, header_text, EDITOR_TEXT_COLOR, 0xffe5dcc8U);

    columns = (content_width > (EDITOR_MARGIN * 2U)) ? (content_width - (EDITOR_MARGIN * 2U)) / GFX_FONT_WIDTH : 0U;
    rows = (text_height > EDITOR_MARGIN) ? (text_height - EDITOR_MARGIN) / (GFX_FONT_HEIGHT + EDITOR_LINE_SPACING) : 0U;
    if (columns == 0U || rows == 0U)
    {
        return;
    }

    cursor_line_start = text_editor_find_line_start(state, state->cursor, columns);
    view_start = cursor_line_start;
    line_count = 1U;
    while (view_start > 0U && line_count < rows)
    {
        unsigned int prev_start;

        prev_start = text_editor_find_prev_line_start(state, view_start, columns);
        if (prev_start == view_start)
        {
            break;
        }
        view_start = prev_start;
        line_count++;
    }

    walk = view_start;
    line_count = 0U;
    while (walk <= state->length && line_count < rows)
    {
        unsigned int draw_x;
        unsigned int draw_y;
        unsigned int count;
        unsigned int index;
        unsigned int cursor_column;
        int cursor_on_line;
        char line_buffer[80];

        draw_x = (unsigned int)x + EDITOR_MARGIN;
        draw_y = text_top + EDITOR_MARGIN + (line_count * (GFX_FONT_HEIGHT + EDITOR_LINE_SPACING));
        count = 0U;
        index = walk;
        cursor_on_line = state->cursor >= walk &&
                         state->cursor <= text_editor_find_next_line_start(state, walk, columns);

        memset(line_buffer, 0, sizeof(line_buffer));
        while (index < state->length &&
               count < columns &&
               count + 1U < sizeof(line_buffer) &&
               state->text[index] != '\n')
        {
            line_buffer[count++] = state->text[index++];
        }

        gfx_draw_string(fb, (int)draw_x, (int)draw_y, line_buffer, EDITOR_TEXT_COLOR, EDITOR_BG_COLOR);

        if (cursor_on_line)
        {
            cursor_column = state->cursor - walk;
            if (cursor_column > count)
            {
                cursor_column = count;
            }

            draw_fill_rect(fb,
                           (int)(draw_x + (cursor_column * GFX_FONT_WIDTH)),
                           (int)draw_y + GFX_FONT_HEIGHT - 2,
                           GFX_FONT_WIDTH,
                           2,
                           EDITOR_CURSOR_COLOR);
        }

        walk = text_editor_find_next_line_start(state, walk, columns);
        if (walk == state->length && state->cursor == state->length && line_count + 1U >= rows)
        {
            break;
        }
        if (walk == state->length && state->length > 0U && state->text[state->length - 1U] != '\n' && count < columns)
        {
            break;
        }
        if (walk == state->length && state->length == 0U)
        {
            break;
        }
        line_count++;
    }

    gfx_draw_string(fb,
                    x + EDITOR_MARGIN,
                    (int)(y + content_height - EDITOR_STATUS_HEIGHT + 3),
                    state->status,
                    EDITOR_MUTED_COLOR,
                    0xffece5d8U);
}

void text_editor_app_event(desktop_app_instance_t *instance,
                           const desktop_window_t *window,
                           const desktop_event_t *event)
{
    text_editor_state_t *state;
    unsigned int columns;

    (void)instance;

    if (event == 0 || window == 0)
    {
        return;
    }

    state = (text_editor_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    if (event->type == DESKTOP_EVENT_WINDOW_CLOSE)
    {
        return;
    }

    columns = (desktop_window_content_width(window) > (EDITOR_MARGIN * 2U))
                  ? (desktop_window_content_width(window) - (EDITOR_MARGIN * 2U)) / GFX_FONT_WIDTH
                  : 1U;
    if (columns == 0U)
    {
        columns = 1U;
    }

    if (event->type == DESKTOP_EVENT_BUTTON_DOWN)
    {
        if (text_editor_save_button_hit_test(window, event->cursor_x, event->cursor_y))
        {
            text_editor_set_status(state, "save not supported yet");
            (void)desktop_show_alert("Save Unsupported", "Saving files not supported yet");
        }
        return;
    }

    if (event->type == DESKTOP_EVENT_KEY &&
        (event->input.pressed == INPUT_KEY_PRESS || event->input.pressed == INPUT_KEY_REPEAT))
    {
        if (event->input.data.keycode == INPUT_KEY_LEFT)
        {
            if (state->cursor > 0U)
            {
                state->cursor--;
            }
            return;
        }
        if (event->input.data.keycode == INPUT_KEY_RIGHT)
        {
            if (state->cursor < state->length)
            {
                state->cursor++;
            }
            return;
        }
        if (event->input.data.keycode == INPUT_KEY_UP)
        {
            state->cursor = text_editor_cursor_prev_line(state, columns);
            return;
        }
        if (event->input.data.keycode == INPUT_KEY_DOWN)
        {
            state->cursor = text_editor_cursor_next_line(state, columns);
            return;
        }
    }

    if (event->type != DESKTOP_EVENT_CHAR)
    {
        return;
    }

    if (event->character == '\b' || event->character == 0x7f)
    {
        text_editor_backspace(state);
        return;
    }

    if (event->character == '\n' || event->character == '\r')
    {
        (void)text_editor_insert_char(state, '\n');
        return;
    }

    if ((unsigned char)event->character < 32U || (unsigned char)event->character > 126U)
    {
        return;
    }

    (void)text_editor_insert_char(state, event->character);
}

int text_editor_app_start(desktop_app_instance_t *instance)
{
    text_editor_state_t *state;

    if (instance == 0)
    {
        return -1;
    }

    state = text_editor_alloc_state();
    if (state == 0)
    {
        return -1;
    }

    text_editor_load_file(state, instance->argument);
    instance->user_data = state;
    return desktop_open_app_instance_window(instance,
                                            "Editor",
                                            EDITOR_WINDOW_WIDTH,
                                            EDITOR_WINDOW_HEIGHT,
                                            state);
}

void text_editor_app_close(desktop_app_instance_t *instance)
{
    text_editor_state_t *state;

    if (instance == 0)
    {
        return;
    }

    state = (text_editor_state_t *)instance->user_data;
    if (state != 0)
    {
        memset(state, 0, sizeof(*state));
    }
}

int text_editor_app_launch_path(const char *path)
{
    return desktop_launch_app_with_argument("editor", path);
}
