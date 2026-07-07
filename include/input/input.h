#ifndef INPUT_INPUT_H
#define INPUT_INPUT_H

#define INPUT_SOURCE_SERIAL 0x1U
#define INPUT_SOURCE_KEYBOARD 0x2U

#define INPUT_EVENT_CHAR 1U
#define INPUT_EVENT_KEY 2U

#define INPUT_KEY_RELEASE 0U
#define INPUT_KEY_PRESS 1U
#define INPUT_KEY_REPEAT 2U

#define INPUT_KEY_1 2U
#define INPUT_KEY_2 3U
#define INPUT_KEY_3 4U
#define INPUT_KEY_4 5U
#define INPUT_KEY_5 6U
#define INPUT_KEY_6 7U
#define INPUT_KEY_7 8U
#define INPUT_KEY_8 9U
#define INPUT_KEY_9 10U
#define INPUT_KEY_0 11U
#define INPUT_KEY_MINUS 12U
#define INPUT_KEY_EQUAL 13U
#define INPUT_KEY_BACKSPACE 14U
#define INPUT_KEY_TAB 15U
#define INPUT_KEY_Q 16U
#define INPUT_KEY_W 17U
#define INPUT_KEY_E 18U
#define INPUT_KEY_R 19U
#define INPUT_KEY_T 20U
#define INPUT_KEY_Y 21U
#define INPUT_KEY_U 22U
#define INPUT_KEY_I 23U
#define INPUT_KEY_O 24U
#define INPUT_KEY_P 25U
#define INPUT_KEY_LEFTBRACE 26U
#define INPUT_KEY_RIGHTBRACE 27U
#define INPUT_KEY_ENTER 28U
#define INPUT_KEY_A 30U
#define INPUT_KEY_S 31U
#define INPUT_KEY_D 32U
#define INPUT_KEY_F 33U
#define INPUT_KEY_G 34U
#define INPUT_KEY_H 35U
#define INPUT_KEY_J 36U
#define INPUT_KEY_K 37U
#define INPUT_KEY_L 38U
#define INPUT_KEY_SEMICOLON 39U
#define INPUT_KEY_APOSTROPHE 40U
#define INPUT_KEY_GRAVE 41U
#define INPUT_KEY_LEFTSHIFT 42U
#define INPUT_KEY_BACKSLASH 43U
#define INPUT_KEY_Z 44U
#define INPUT_KEY_X 45U
#define INPUT_KEY_C 46U
#define INPUT_KEY_V 47U
#define INPUT_KEY_B 48U
#define INPUT_KEY_N 49U
#define INPUT_KEY_M 50U
#define INPUT_KEY_COMMA 51U
#define INPUT_KEY_DOT 52U
#define INPUT_KEY_SLASH 53U
#define INPUT_KEY_RIGHTSHIFT 54U
#define INPUT_KEY_SPACE 57U

typedef union input_event_data
{
    unsigned short keycode;
    char character;
} input_event_data_t;

typedef struct input_event
{
    unsigned int type;
    unsigned int source;
    input_event_data_t data;
    unsigned char pressed;
    unsigned char modifiers;
} input_event_t;

void input_init(void);
void input_poll(void);
void input_set_mode(unsigned int mode);
unsigned int input_mode(void);
int input_keyboard_available(void);
int input_push_event(const input_event_t *event);
int input_pop_event(input_event_t *event_out);
int input_has_event(void);
int input_event_to_char(const input_event_t *event, char *ch_out);
void input_queue_serial_char(char ch);
void input_queue_key_event(unsigned short keycode, unsigned int pressed);
char input_getc(void);

#endif
