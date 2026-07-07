#include "desktop/desktop_input.h"
#define DESKTOP_CURSOR_STEP 8U

static unsigned int desktop_find_topmost_window(const desktop_window_t *windows,
                                                unsigned int window_count,
                                                unsigned int x,
                                                unsigned int y)
{
    unsigned int index;

    for (index = window_count; index > 0U; index--)
    {
        if (desktop_window_contains_point(&windows[index - 1U], x, y))
        {
            return index - 1U;
        }
    }

    return window_count;
}

static unsigned int desktop_find_next_focusable_window(const desktop_window_t *windows,
                                                       unsigned int window_count,
                                                       unsigned int focused_window_index)
{
    unsigned int offset;

    if (window_count == 0U)
    {
        return 0U;
    }

    for (offset = 1U; offset <= window_count; offset++)
    {
        unsigned int index;

        index = (focused_window_index + offset) % window_count;
        if (desktop_window_is_visible(&windows[index]))
        {
            return index;
        }
    }

    return focused_window_index;
}

static int desktop_input_is_key_pressed(const input_event_t *input_event)
{
    return input_event->pressed == INPUT_KEY_PRESS || input_event->pressed == INPUT_KEY_REPEAT;
}

static int desktop_input_char_matches(char ch, char lower, char upper)
{
    return ch == lower || ch == upper;
}

static void desktop_clamp_cursor(unsigned int fb_width,
                                 unsigned int fb_height,
                                 unsigned int *cursor_x,
                                 unsigned int *cursor_y)
{
    if (fb_width == 0U || fb_height == 0U)
    {
        *cursor_x = 0U;
        *cursor_y = 0U;
        return;
    }

    if (*cursor_x >= fb_width)
    {
        *cursor_x = fb_width - 1U;
    }
    if (*cursor_y >= fb_height)
    {
        *cursor_y = fb_height - 1U;
    }
}

void desktop_input_reset(unsigned int fb_width,
                         unsigned int fb_height,
                         unsigned int *cursor_x,
                         unsigned int *cursor_y,
                         unsigned int *focused_window_index,
                         unsigned int *primary_button_down)
{
    if (cursor_x != 0)
    {
        *cursor_x = fb_width / 2U;
    }
    if (cursor_y != 0)
    {
        *cursor_y = fb_height / 2U;
    }
    if (focused_window_index != 0)
    {
        *focused_window_index = 0U;
    }
    if (primary_button_down != 0)
    {
        *primary_button_down = 0U;
    }
}

unsigned int desktop_input_route(const input_event_t *input_event,
                                 desktop_window_t *windows,
                                 unsigned int window_count,
                                 unsigned int fb_width,
                                 unsigned int fb_height,
                                 unsigned int *cursor_x,
                                 unsigned int *cursor_y,
                                 unsigned int *focused_window_index,
                                 unsigned int *primary_button_down,
                                 desktop_event_t *event_out)
{
    unsigned int result;
    unsigned int index;

    result = DESKTOP_INPUT_RESULT_NONE;
    desktop_event_init(event_out);
    if (input_event == 0 || windows == 0 || cursor_x == 0 || cursor_y == 0 ||
        focused_window_index == 0 || primary_button_down == 0)
    {
        return result;
    }

    if (input_event->type == INPUT_EVENT_CHAR)
    {
        unsigned int next_focus_index;

        if (input_event->data.character == 27)
        {
            return DESKTOP_INPUT_RESULT_EXIT;
        }

        if (input_event->data.character == '\t')
        {
            next_focus_index = desktop_find_next_focusable_window(windows,
                                                                  window_count,
                                                                  *focused_window_index);
            desktop_event_from_input(event_out, input_event);
            event_out->type = DESKTOP_EVENT_CHAR;
            event_out->target_window_id = (next_focus_index < window_count)
                                              ? windows[next_focus_index].id
                                              : 0U;
            event_out->cursor_x = *cursor_x;
            event_out->cursor_y = *cursor_y;
            return DESKTOP_INPUT_RESULT_REDRAW;
        }

        if (desktop_input_char_matches(input_event->data.character, 'a', 'A'))
        {
            if (*cursor_x > DESKTOP_CURSOR_STEP)
            {
                *cursor_x -= DESKTOP_CURSOR_STEP;
            }
            else
            {
                *cursor_x = 0U;
            }
            desktop_clamp_cursor(fb_width, fb_height, cursor_x, cursor_y);
            event_out->type = DESKTOP_EVENT_CURSOR_MOVE;
            result = DESKTOP_INPUT_RESULT_REDRAW;
        }
        else if (desktop_input_char_matches(input_event->data.character, 'd', 'D'))
        {
            *cursor_x += DESKTOP_CURSOR_STEP;
            desktop_clamp_cursor(fb_width, fb_height, cursor_x, cursor_y);
            event_out->type = DESKTOP_EVENT_CURSOR_MOVE;
            result = DESKTOP_INPUT_RESULT_REDRAW;
        }
        else if (desktop_input_char_matches(input_event->data.character, 'w', 'W'))
        {
            if (*cursor_y > DESKTOP_CURSOR_STEP)
            {
                *cursor_y -= DESKTOP_CURSOR_STEP;
            }
            else
            {
                *cursor_y = 0U;
            }
            desktop_clamp_cursor(fb_width, fb_height, cursor_x, cursor_y);
            event_out->type = DESKTOP_EVENT_CURSOR_MOVE;
            result = DESKTOP_INPUT_RESULT_REDRAW;
        }
        else if (desktop_input_char_matches(input_event->data.character, 's', 'S'))
        {
            *cursor_y += DESKTOP_CURSOR_STEP;
            desktop_clamp_cursor(fb_width, fb_height, cursor_x, cursor_y);
            event_out->type = DESKTOP_EVENT_CURSOR_MOVE;
            result = DESKTOP_INPUT_RESULT_REDRAW;
        }
        else if (input_event->data.character == '\n' || input_event->data.character == '\r')
        {
            index = desktop_find_topmost_window(windows, window_count, *cursor_x, *cursor_y);
            if (index < window_count)
            {
                *focused_window_index = index;
            }

            if (*primary_button_down == 0U)
            {
                *primary_button_down = 1U;
                event_out->type = DESKTOP_EVENT_BUTTON_DOWN;
            }
            else
            {
                *primary_button_down = 0U;
                event_out->type = DESKTOP_EVENT_BUTTON_UP;
            }

            event_out->target_window_id = (index < window_count) ? windows[index].id : 0U;
            event_out->cursor_x = *cursor_x;
            event_out->cursor_y = *cursor_y;
            return DESKTOP_INPUT_RESULT_REDRAW;
        }

        if (result != DESKTOP_INPUT_RESULT_NONE)
        {
            event_out->target_window_id = (*focused_window_index < window_count)
                                              ? windows[*focused_window_index].id
                                              : 0U;
            event_out->cursor_x = *cursor_x;
            event_out->cursor_y = *cursor_y;
            return result;
        }

        desktop_event_from_input(event_out, input_event);
        event_out->target_window_id = (*focused_window_index < window_count)
                                          ? windows[*focused_window_index].id
                                          : 0U;
        event_out->cursor_x = *cursor_x;
        event_out->cursor_y = *cursor_y;
        return DESKTOP_INPUT_RESULT_REDRAW;
    }

    if (input_event->type != INPUT_EVENT_KEY)
    {
        return result;
    }

    if (input_event->data.keycode == INPUT_KEY_ESC)
    {
        if (desktop_input_is_key_pressed(input_event))
        {
            return DESKTOP_INPUT_RESULT_EXIT;
        }
        return DESKTOP_INPUT_RESULT_NONE;
    }

    if (input_event->data.keycode == INPUT_KEY_ENTER ||
        input_event->data.keycode == INPUT_KEY_MOUSE_LEFT)
    {
        index = desktop_find_topmost_window(windows, window_count, *cursor_x, *cursor_y);
        desktop_event_from_input(event_out, input_event);
        event_out->type = desktop_input_is_key_pressed(input_event)
                              ? DESKTOP_EVENT_BUTTON_DOWN
                              : DESKTOP_EVENT_BUTTON_UP;
        *primary_button_down = desktop_input_is_key_pressed(input_event) ? 1U : 0U;
        event_out->target_window_id = (index < window_count) ? windows[index].id : 0U;
        event_out->cursor_x = *cursor_x;
        event_out->cursor_y = *cursor_y;
        return DESKTOP_INPUT_RESULT_REDRAW;
    }

    if (!desktop_input_is_key_pressed(input_event))
    {
        if (*focused_window_index < window_count)
        {
            desktop_event_from_input(event_out, input_event);
            event_out->target_window_id = windows[*focused_window_index].id;
            event_out->cursor_x = *cursor_x;
            event_out->cursor_y = *cursor_y;
            return DESKTOP_INPUT_RESULT_REDRAW;
        }
        return DESKTOP_INPUT_RESULT_NONE;
    }

    if (input_event->data.keycode == INPUT_KEY_TAB)
    {
        index = desktop_find_next_focusable_window(windows,
                                                   window_count,
                                                   *focused_window_index);
        desktop_event_from_input(event_out, input_event);
        event_out->type = DESKTOP_EVENT_KEY;
        event_out->target_window_id = (index < window_count) ? windows[index].id : 0U;
        event_out->cursor_x = *cursor_x;
        event_out->cursor_y = *cursor_y;
        return DESKTOP_INPUT_RESULT_REDRAW;
    }
    else if (input_event->data.keycode == INPUT_KEY_LEFT ||
             input_event->data.keycode == INPUT_KEY_A)
    {
        if (*cursor_x > DESKTOP_CURSOR_STEP)
        {
            *cursor_x -= DESKTOP_CURSOR_STEP;
        }
        else
        {
            *cursor_x = 0U;
        }
        result |= DESKTOP_INPUT_RESULT_REDRAW;
    }
    else if (input_event->data.keycode == INPUT_KEY_RIGHT ||
             input_event->data.keycode == INPUT_KEY_D)
    {
        *cursor_x += DESKTOP_CURSOR_STEP;
        result |= DESKTOP_INPUT_RESULT_REDRAW;
    }
    else if (input_event->data.keycode == INPUT_KEY_UP ||
             input_event->data.keycode == INPUT_KEY_W)
    {
        if (*cursor_y > DESKTOP_CURSOR_STEP)
        {
            *cursor_y -= DESKTOP_CURSOR_STEP;
        }
        else
        {
            *cursor_y = 0U;
        }
        result |= DESKTOP_INPUT_RESULT_REDRAW;
    }
    else if (input_event->data.keycode == INPUT_KEY_DOWN ||
             input_event->data.keycode == INPUT_KEY_S)
    {
        *cursor_y += DESKTOP_CURSOR_STEP;
        result |= DESKTOP_INPUT_RESULT_REDRAW;
    }
    desktop_clamp_cursor(fb_width, fb_height, cursor_x, cursor_y);

    if (result != DESKTOP_INPUT_RESULT_NONE)
    {
        event_out->type = DESKTOP_EVENT_CURSOR_MOVE;
        event_out->cursor_x = *cursor_x;
        event_out->cursor_y = *cursor_y;
        event_out->target_window_id = (*focused_window_index < window_count)
                                          ? windows[*focused_window_index].id
                                          : 0U;
        return result;
    }

    desktop_event_from_input(event_out, input_event);
    event_out->target_window_id = (*focused_window_index < window_count)
                                      ? windows[*focused_window_index].id
                                      : 0U;
    event_out->cursor_x = *cursor_x;
    event_out->cursor_y = *cursor_y;
    return DESKTOP_INPUT_RESULT_REDRAW;
}
