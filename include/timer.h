#ifndef TIMER_H
#define TIMER_H

#include "type.h"
#include "isr.h"

void init_timer(u32 freq);
u32 get_ticks();
void sleep(u32 ticks);

#endif
