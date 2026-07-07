#include "drivers/uart.h"
#include "drivers/virtio_input.h"
#include "kernel/input.h"

#define INPUT_QUEUE_CAPACITY 64U
#define INPUT_MOD_SHIFT 0x01U

static input_event_t input_queue[INPUT_QUEUE_CAPACITY];
static unsigned int input_queue_head;
static unsigned int input_queue_tail;
static unsigned int input_mode_flags;
static unsigned int input_modifiers;

static int input_queue_is_empty(void)
{
    return input_queue_head == input_queue_tail;
}

static int input_queue_is_full(void)
{
    return ((input_queue_tail + 1U) % INPUT_QUEUE_CAPACITY) == input_queue_head;
}

static int input_event_allowed(const input_event_t *event)
{
    if (event->source == INPUT_SOURCE_SERIAL)
    {
        return (input_mode_flags & INPUT_SOURCE_SERIAL) != 0U;
    }

    if (event->source == INPUT_SOURCE_KEYBOARD)
    {
        return (input_mode_flags & INPUT_SOURCE_KEYBOARD) != 0U;
    }

    return 0;
}

static void input_queue_push(const input_event_t *event)
{
    if (input_queue_is_full())
    {
        input_queue_head = (input_queue_head + 1U) % INPUT_QUEUE_CAPACITY;
    }

    input_queue[input_queue_tail] = *event;
    input_queue_tail = (input_queue_tail + 1U) % INPUT_QUEUE_CAPACITY;
}

static int input_key_is_shift(unsigned short code)
{
    return code == INPUT_KEY_LEFTSHIFT || code == INPUT_KEY_RIGHTSHIFT;
}

static char input_translate_alpha(unsigned short code, unsigned int shifted)
{
    switch (code)
    {
    case INPUT_KEY_A: return shifted != 0U ? 'A' : 'a';
    case INPUT_KEY_B: return shifted != 0U ? 'B' : 'b';
    case INPUT_KEY_C: return shifted != 0U ? 'C' : 'c';
    case INPUT_KEY_D: return shifted != 0U ? 'D' : 'd';
    case INPUT_KEY_E: return shifted != 0U ? 'E' : 'e';
    case INPUT_KEY_F: return shifted != 0U ? 'F' : 'f';
    case INPUT_KEY_G: return shifted != 0U ? 'G' : 'g';
    case INPUT_KEY_H: return shifted != 0U ? 'H' : 'h';
    case INPUT_KEY_I: return shifted != 0U ? 'I' : 'i';
    case INPUT_KEY_J: return shifted != 0U ? 'J' : 'j';
    case INPUT_KEY_K: return shifted != 0U ? 'K' : 'k';
    case INPUT_KEY_L: return shifted != 0U ? 'L' : 'l';
    case INPUT_KEY_M: return shifted != 0U ? 'M' : 'm';
    case INPUT_KEY_N: return shifted != 0U ? 'N' : 'n';
    case INPUT_KEY_O: return shifted != 0U ? 'O' : 'o';
    case INPUT_KEY_P: return shifted != 0U ? 'P' : 'p';
    case INPUT_KEY_Q: return shifted != 0U ? 'Q' : 'q';
    case INPUT_KEY_R: return shifted != 0U ? 'R' : 'r';
    case INPUT_KEY_S: return shifted != 0U ? 'S' : 's';
    case INPUT_KEY_T: return shifted != 0U ? 'T' : 't';
    case INPUT_KEY_U: return shifted != 0U ? 'U' : 'u';
    case INPUT_KEY_V: return shifted != 0U ? 'V' : 'v';
    case INPUT_KEY_W: return shifted != 0U ? 'W' : 'w';
    case INPUT_KEY_X: return shifted != 0U ? 'X' : 'x';
    case INPUT_KEY_Y: return shifted != 0U ? 'Y' : 'y';
    case INPUT_KEY_Z: return shifted != 0U ? 'Z' : 'z';
    default:
        return '\0';
    }
}

static char input_translate_digit(unsigned short code, unsigned int shifted)
{
    switch (code)
    {
    case INPUT_KEY_1: return shifted != 0U ? '!' : '1';
    case INPUT_KEY_2: return shifted != 0U ? '@' : '2';
    case INPUT_KEY_3: return shifted != 0U ? '#' : '3';
    case INPUT_KEY_4: return shifted != 0U ? '$' : '4';
    case INPUT_KEY_5: return shifted != 0U ? '%' : '5';
    case INPUT_KEY_6: return shifted != 0U ? '^' : '6';
    case INPUT_KEY_7: return shifted != 0U ? '&' : '7';
    case INPUT_KEY_8: return shifted != 0U ? '*' : '8';
    case INPUT_KEY_9: return shifted != 0U ? '(' : '9';
    case INPUT_KEY_0: return shifted != 0U ? ')' : '0';
    default:
        return '\0';
    }
}

static char input_translate_punctuation(unsigned short code, unsigned int shifted)
{
    switch (code)
    {
    case INPUT_KEY_SPACE: return ' ';
    case INPUT_KEY_ENTER: return '\n';
    case INPUT_KEY_BACKSPACE: return '\b';
    case INPUT_KEY_TAB: return '\t';
    case INPUT_KEY_MINUS: return shifted != 0U ? '_' : '-';
    case INPUT_KEY_EQUAL: return shifted != 0U ? '+' : '=';
    case INPUT_KEY_LEFTBRACE: return shifted != 0U ? '{' : '[';
    case INPUT_KEY_RIGHTBRACE: return shifted != 0U ? '}' : ']';
    case INPUT_KEY_SEMICOLON: return shifted != 0U ? ':' : ';';
    case INPUT_KEY_APOSTROPHE: return shifted != 0U ? '"' : '\'';
    case INPUT_KEY_GRAVE: return shifted != 0U ? '~' : '`';
    case INPUT_KEY_BACKSLASH: return shifted != 0U ? '|' : '\\';
    case INPUT_KEY_COMMA: return shifted != 0U ? '<' : ',';
    case INPUT_KEY_DOT: return shifted != 0U ? '>' : '.';
    case INPUT_KEY_SLASH: return shifted != 0U ? '?' : '/';
    default:
        return '\0';
    }
}

static char input_translate_keypress(unsigned short code)
{
    char ch;
    unsigned int shifted;

    shifted = (input_modifiers & INPUT_MOD_SHIFT) != 0U;

    ch = input_translate_alpha(code, shifted);
    if (ch != '\0')
    {
        return ch;
    }

    ch = input_translate_digit(code, shifted);
    if (ch != '\0')
    {
        return ch;
    }

    return input_translate_punctuation(code, shifted);
}

static void input_poll_sources(void)
{
    if ((input_mode_flags & INPUT_SOURCE_SERIAL) != 0U && uart_can_read())
    {
        input_queue_serial_char(uart_getc());
    }

    if ((input_mode_flags & INPUT_SOURCE_KEYBOARD) != 0U && virtio_input_available())
    {
        virtio_input_poll();
    }
}

void input_init(void)
{
    input_queue_head = 0U;
    input_queue_tail = 0U;
    input_mode_flags = INPUT_SOURCE_SERIAL;
    input_modifiers = 0U;
}

void input_set_mode(unsigned int mode)
{
    input_mode_flags = mode & (INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD);
    if (input_mode_flags == 0U)
    {
        input_mode_flags = INPUT_SOURCE_SERIAL;
    }
}

unsigned int input_mode(void)
{
    return input_mode_flags;
}

int input_keyboard_available(void)
{
    return virtio_input_available();
}

void input_queue_serial_char(char ch)
{
    input_event_t event;

    event.type = INPUT_EVENT_CHAR;
    event.source = INPUT_SOURCE_SERIAL;
    event.data.ch = ch;
    input_queue_push(&event);
}

void input_queue_key_event(unsigned short code, unsigned int value)
{
    input_event_t event;

    event.type = INPUT_EVENT_KEY;
    event.source = INPUT_SOURCE_KEYBOARD;
    event.data.key.code = code;
    event.data.key.state = (unsigned char)value;
    event.data.key.modifiers = (unsigned char)input_modifiers;
    input_queue_push(&event);
}

int input_pop_event(input_event_t *event_out)
{
    if (event_out == 0 || input_queue_is_empty())
    {
        return 0;
    }

    *event_out = input_queue[input_queue_head];
    input_queue_head = (input_queue_head + 1U) % INPUT_QUEUE_CAPACITY;
    return 1;
}

char input_getc(void)
{
    input_event_t event;

    for (;;)
    {
        while (!input_pop_event(&event))
        {
            input_poll_sources();
        }

        if (!input_event_allowed(&event))
        {
            continue;
        }

        if (event.type == INPUT_EVENT_CHAR)
        {
            return event.data.ch;
        }

        if (event.type == INPUT_EVENT_KEY)
        {
            if (input_key_is_shift(event.data.key.code))
            {
                if (event.data.key.state == INPUT_KEY_RELEASE)
                {
                    input_modifiers &= ~INPUT_MOD_SHIFT;
                }
                else
                {
                    input_modifiers |= INPUT_MOD_SHIFT;
                }
                continue;
            }

            if (event.data.key.state == INPUT_KEY_PRESS || event.data.key.state == INPUT_KEY_REPEAT)
            {
                char ch;

                ch = input_translate_keypress(event.data.key.code);
                if (ch != '\0')
                {
                    return ch;
                }
            }
        }
    }
}
