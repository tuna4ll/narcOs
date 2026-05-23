#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>
void init_mouse();
void handle_mouse();
int get_mouse_x();
int get_mouse_y();
int mouse_left_pressed();
int mouse_right_pressed();
int mouse_buttons_state();
void mouse_set_position(int x, int y);
void mouse_consume_delta(int* out_dx, int* out_dy);
int mouse_consume_moved();
int mouse_consume_wheel();
#endif
