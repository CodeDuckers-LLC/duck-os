#ifndef DESKTOP_DESKTOP_INPUT_H
#define DESKTOP_DESKTOP_INPUT_H

#include "desktop/desktop_event.h"
#include "desktop/desktop_window.h"
#include "input/input.h"

#define DESKTOP_INPUT_RESULT_NONE 0U
#define DESKTOP_INPUT_RESULT_REDRAW 0x1U
#define DESKTOP_INPUT_RESULT_EXIT 0x2U

void desktop_input_reset(unsigned int fb_width,
                         unsigned int fb_height,
                         unsigned int *cursor_x,
                         unsigned int *cursor_y,
                         unsigned int *focused_window_index,
                         unsigned int *primary_button_down);
unsigned int desktop_input_route(const input_event_t *input_event,
                                 desktop_window_t *windows,
                                 unsigned int window_count,
                                 unsigned int fb_width,
                                 unsigned int fb_height,
                                 int allow_exit,
                                 unsigned int *cursor_x,
                                 unsigned int *cursor_y,
                                 unsigned int *focused_window_index,
                                 unsigned int *primary_button_down,
                                 desktop_event_t *event_out);

#endif
