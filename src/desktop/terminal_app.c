#include "desktop/terminal_app.h"
#include "desktop/desktop.h"
#include "desktop/desktop_event.h"
#include "desktop/desktop_window.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "kernel/shell.h"
#include "lib/string.h"

#define TERMINAL_WINDOW_WIDTH 560U
#define TERMINAL_WINDOW_HEIGHT 220U
#define TERMINAL_SCROLLBACK_LINES 128U
#define TERMINAL_LINE_MAX 64U
#define TERMINAL_INPUT_MAX 64U
#define TERMINAL_PROMPT "duck-os@dev > "
#define TERMINAL_BG_COLOR 0xff0f151aU
#define TERMINAL_FG_COLOR 0xffd7e3eaU
#define TERMINAL_ACCENT_COLOR 0xff69b7d4U
#define TERMINAL_LINE_SPACING 2
#define TERMINAL_MAX_INSTANCES 8U

typedef struct terminal_app_state
{
    char lines[TERMINAL_SCROLLBACK_LINES][TERMINAL_LINE_MAX + 1U];
    unsigned int line_count;
    unsigned int line_head;
    unsigned int current_output_length;
    char input[TERMINAL_INPUT_MAX];
    unsigned int input_length;
    int active;
} terminal_app_state_t;

static terminal_app_state_t terminal_states[TERMINAL_MAX_INSTANCES];

static void terminal_app_append_char(terminal_app_state_t *state, char ch);
static void terminal_app_append_text(terminal_app_state_t *state, const char *text, unsigned long length);
static void terminal_app_begin_new_line(terminal_app_state_t *state);
static char *terminal_app_current_line(terminal_app_state_t *state);
static const char *terminal_app_line_at(const terminal_app_state_t *state, unsigned int index);
static void terminal_app_shell_output(const char *text, unsigned long length, void *user_data);
static terminal_app_state_t *terminal_app_alloc_state(void);

static terminal_app_state_t *terminal_app_alloc_state(void)
{
    unsigned int index;

    for (index = 0; index < TERMINAL_MAX_INSTANCES; index++)
    {
        if (!terminal_states[index].active)
        {
            memset(&terminal_states[index], 0, sizeof(terminal_states[index]));
            terminal_states[index].active = 1;
            terminal_app_begin_new_line(&terminal_states[index]);
            return &terminal_states[index];
        }
    }

    return 0;
}

static char *terminal_app_current_line(terminal_app_state_t *state)
{
    if (state->line_count == 0U)
    {
        terminal_app_begin_new_line(state);
    }

    return state->lines[(state->line_head + state->line_count - 1U) % TERMINAL_SCROLLBACK_LINES];
}

static const char *terminal_app_line_at(const terminal_app_state_t *state, unsigned int index)
{
    return state->lines[(state->line_head + index) % TERMINAL_SCROLLBACK_LINES];
}

static void terminal_app_begin_new_line(terminal_app_state_t *state)
{
    unsigned int index;

    if (state->line_count < TERMINAL_SCROLLBACK_LINES)
    {
        index = (state->line_head + state->line_count) % TERMINAL_SCROLLBACK_LINES;
        state->line_count++;
    }
    else
    {
        index = state->line_head;
        state->line_head = (state->line_head + 1U) % TERMINAL_SCROLLBACK_LINES;
    }

    memset(state->lines[index], 0, sizeof(state->lines[index]));
    state->current_output_length = 0U;
}

static void terminal_app_append_char(terminal_app_state_t *state, char ch)
{
    char *line;

    if (ch == '\r')
    {
        return;
    }

    if (ch == '\n')
    {
        terminal_app_begin_new_line(state);
        return;
    }

    if (ch == '\t')
    {
        terminal_app_append_char(state, ' ');
        terminal_app_append_char(state, ' ');
        terminal_app_append_char(state, ' ');
        terminal_app_append_char(state, ' ');
        return;
    }

    if ((unsigned char)ch < 32U || (unsigned char)ch > 126U)
    {
        ch = '?';
    }

    if (state->current_output_length >= TERMINAL_LINE_MAX)
    {
        terminal_app_begin_new_line(state);
    }

    line = terminal_app_current_line(state);
    line[state->current_output_length++] = ch;
    line[state->current_output_length] = '\0';
}

static void terminal_app_append_text(terminal_app_state_t *state, const char *text, unsigned long length)
{
    unsigned long index;

    for (index = 0; index < length; index++)
    {
        terminal_app_append_char(state, text[index]);
    }
}

static void terminal_app_shell_output(const char *text, unsigned long length, void *user_data)
{
    terminal_app_state_t *state;

    state = (terminal_app_state_t *)user_data;
    if (state == 0 || text == 0)
    {
        return;
    }

    terminal_app_append_text(state, text, length);
}

void terminal_app_render(desktop_app_instance_t *instance,
                         const desktop_window_t *window,
                         framebuffer_t *fb)
{
    terminal_app_state_t *state;
    unsigned int visible_rows;
    unsigned int total_rows;
    unsigned int start_row;
    unsigned int row;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    char prompt_line[sizeof(TERMINAL_PROMPT) + TERMINAL_INPUT_MAX];

    (void)instance;

    if (window == 0)
    {
        return;
    }

    state = (terminal_app_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    x = desktop_window_content_x(window);
    y = desktop_window_content_y(window);
    width = desktop_window_content_width(window);
    height = desktop_window_content_height(window);

    draw_fill_rect(fb, x, y, (int)width, (int)height, TERMINAL_BG_COLOR);
    draw_rect(fb, x, y, (int)width, (int)height, TERMINAL_ACCENT_COLOR);

    visible_rows = height / (GFX_FONT_HEIGHT + TERMINAL_LINE_SPACING);
    if (visible_rows == 0U)
    {
        return;
    }

    total_rows = state->line_count + 1U;
    start_row = 0U;
    if (total_rows > visible_rows)
    {
        start_row = total_rows - visible_rows;
    }

    for (row = start_row; row < state->line_count; row++)
    {
        unsigned int draw_row;

        draw_row = row - start_row;
        gfx_draw_string(fb,
                        x + 6,
                        y + 6 + (int)(draw_row * (GFX_FONT_HEIGHT + TERMINAL_LINE_SPACING)),
                        terminal_app_line_at(state, row),
                        TERMINAL_FG_COLOR,
                        TERMINAL_BG_COLOR);
    }

    memset(prompt_line, 0, sizeof(prompt_line));
    strlcpy(prompt_line, TERMINAL_PROMPT, sizeof(prompt_line));
    strlcpy(prompt_line + strlen(prompt_line),
            state->input,
            sizeof(prompt_line) - strlen(prompt_line));

    gfx_draw_string(fb,
                    x + 6,
                    y + 6 + (int)((total_rows - start_row - 1U) * (GFX_FONT_HEIGHT + TERMINAL_LINE_SPACING)),
                    prompt_line,
                    TERMINAL_FG_COLOR,
                    TERMINAL_BG_COLOR);
}

void terminal_app_event(desktop_app_instance_t *instance,
                        const desktop_window_t *window,
                        const desktop_event_t *event)
{
    terminal_app_state_t *state;
    char prompt_line[sizeof(TERMINAL_PROMPT) + TERMINAL_INPUT_MAX];

    (void)instance;

    if (window == 0 || event == 0)
    {
        return;
    }

    state = (terminal_app_state_t *)window->user_data;
    if (state == 0)
    {
        return;
    }

    if (event->type == DESKTOP_EVENT_WINDOW_CLOSE)
    {
        return;
    }

    if (event->type == DESKTOP_EVENT_KEY &&
        event->input.pressed == INPUT_KEY_PRESS &&
        event->input.data.keycode == INPUT_KEY_BACKSPACE)
    {
        if (state->input_length > 0U)
        {
            state->input_length--;
            state->input[state->input_length] = '\0';
        }
        return;
    }

    if (event->type != DESKTOP_EVENT_CHAR)
    {
        return;
    }

    if (event->character == '\b' || event->character == 0x7f)
    {
        if (state->input_length > 0U)
        {
            state->input_length--;
            state->input[state->input_length] = '\0';
        }
        return;
    }

    if (event->character == '\n' || event->character == '\r')
    {
        memset(prompt_line, 0, sizeof(prompt_line));
        strlcpy(prompt_line, TERMINAL_PROMPT, sizeof(prompt_line));
        strlcpy(prompt_line + strlen(prompt_line),
                state->input,
                sizeof(prompt_line) - strlen(prompt_line));
        terminal_app_append_text(state, prompt_line, strlen(prompt_line));
        terminal_app_begin_new_line(state);
        shell_execute_line(state->input, terminal_app_shell_output, state);
        state->input_length = 0U;
        state->input[0] = '\0';
        return;
    }

    if ((unsigned char)event->character < 32U || (unsigned char)event->character > 126U)
    {
        return;
    }

    if ((state->input_length + 1U) >= TERMINAL_INPUT_MAX)
    {
        return;
    }

    state->input[state->input_length++] = event->character;
    state->input[state->input_length] = '\0';
}

int terminal_app_start(desktop_app_instance_t *instance)
{
    terminal_app_state_t *state;

    if (instance == 0)
    {
        return -1;
    }

    state = terminal_app_alloc_state();
    if (state == 0)
    {
        return -1;
    }

    instance->user_data = state;
    return desktop_open_app_instance_window(instance,
                                            "Terminal",
                                            TERMINAL_WINDOW_WIDTH,
                                            TERMINAL_WINDOW_HEIGHT,
                                            state);
}

void terminal_app_close(desktop_app_instance_t *instance)
{
    terminal_app_state_t *state;

    if (instance == 0)
    {
        return;
    }

    state = (terminal_app_state_t *)instance->user_data;
    if (state != 0)
    {
        memset(state, 0, sizeof(*state));
    }
}
