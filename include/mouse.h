#ifndef MOUSE_H
#define MOUSE_H

#include "type.h"
#include "isr.h"

void init_mouse();
u32 mouse_callback(registers_t *regs);

#endif
