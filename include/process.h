#ifndef PROCESS_H
#define PROCESS_H

#include "type.h"

#define MAX_PROCESSES 16
#define STACK_SIZE 4096

typedef enum {
    PROCESS_EMPTY,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_DEAD
} process_state_t;

typedef struct {
    u32 id;
    volatile process_state_t state;
    u32 esp; // Stack pointer
    u32 stack_base; // Base of the allocated stack
    volatile u32 sleep_ticks;
} process_t;

void init_scheduler();
int create_process(void (*entry_point)());
u32 schedule(u32 esp);
void process_sleep(u32 ticks);
void process_exit();
u32 get_current_process_id();

#endif
