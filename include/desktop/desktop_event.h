#ifndef DESKTOP_DESKTOP_EVENT_H
#define DESKTOP_DESKTOP_EVENT_H

#include "input/input.h"

#define DESKTOP_EVENT_NONE 0U
#define DESKTOP_EVENT_REDRAW 1U
#define DESKTOP_EVENT_KEY 2U
#define DESKTOP_EVENT_CHAR 3U
#define DESKTOP_EVENT_BUTTON_DOWN 4U
#define DESKTOP_EVENT_BUTTON_UP 5U
#define DESKTOP_EVENT_CURSOR_MOVE 6U
#define DESKTOP_EVENT_WINDOW_CLOSE 7U

typedef struct desktop_event
{
    unsigned int type;
    input_event_t input;
    char character;
    unsigned int target_window_id;
    unsigned int cursor_x;
    unsigned int cursor_y;
} desktop_event_t;

void desktop_event_init(desktop_event_t *event);
void desktop_event_from_input(desktop_event_t *event, const input_event_t *input_event);

#endif
