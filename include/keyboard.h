#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "type.h"
#include "isr.h"

void init_keyboard();
char get_last_char();
void clear_last_char();

#endif
