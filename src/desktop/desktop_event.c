#include "desktop/desktop_event.h"
#include "lib/string.h"

void desktop_event_init(desktop_event_t *event)
{
    if (event == 0)
    {
        return;
    }

    memset(event, 0, sizeof(*event));
}

void desktop_event_from_input(desktop_event_t *event, const input_event_t *input_event)
{
    desktop_event_init(event);
    if (event == 0 || input_event == 0)
    {
        return;
    }

    event->input = *input_event;
    if (input_event->type == INPUT_EVENT_CHAR)
    {
        event->type = DESKTOP_EVENT_CHAR;
        event->character = input_event->data.character;
        return;
    }

    if (input_event->type == INPUT_EVENT_KEY)
    {
        event->type = DESKTOP_EVENT_KEY;
    }
}
