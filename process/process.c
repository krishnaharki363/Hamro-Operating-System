#include "../include/process.h"
#include "../include/mem.h"
#include "../include/timer.h"
#include "../include/isr.h"

process_t processes[MAX_PROCESSES];
int current_process_idx = -1;
int process_count = 0;
int next_pid = 1;

void init_scheduler() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROCESS_EMPTY;
    }
    // Main kernel thread
    processes[0].id = 0;
    processes[0].state = PROCESS_RUNNING;
    current_process_idx = 0;
    process_count = 1;
}

int create_process(void (*entry_point)()) {
    if (process_count >= MAX_PROCESSES) return -1;

    int p_idx = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_EMPTY || processes[i].state == PROCESS_DEAD) {
            p_idx = i;
            break;
        }
    }

    if (p_idx == -1) return -1;

    u32 stack_base = (u32)kmalloc(STACK_SIZE);
    if (!stack_base) return -1;

    u32* stack = (u32*)(stack_base + STACK_SIZE);

    *(--stack) = 0x202; // EFLAGS (Interrupts enabled)
    *(--stack) = 0x08;  // CS
    *(--stack) = (u32)entry_point; // EIP
    *(--stack) = 0; // err_code
    *(--stack) = 0; // int_no

    // Pusha: eax, ecx, edx, ebx, esp, ebp, esi, edi
    for(int i=0; i<8; i++) *(--stack) = 0;
    
    *(--stack) = 0x10; // ds

    processes[p_idx].id = next_pid++;
    processes[p_idx].state = PROCESS_READY;
    processes[p_idx].esp = (u32)stack;
    processes[p_idx].stack_base = stack_base;
    processes[p_idx].sleep_ticks = 0;

    process_count++;
    return processes[p_idx].id;
}

u32 schedule(u32 esp) {
    if (current_process_idx == -1) return esp;

    processes[current_process_idx].esp = esp;
    
    if (processes[current_process_idx].state == PROCESS_RUNNING) {
        processes[current_process_idx].state = PROCESS_READY;
    }

    int next_idx = (current_process_idx + 1) % MAX_PROCESSES;
    int found = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[next_idx].state == PROCESS_SLEEPING) {
            if (get_ticks() >= processes[next_idx].sleep_ticks) {
                processes[next_idx].state = PROCESS_READY;
            }
        }

        if (processes[next_idx].state == PROCESS_READY) {
            found = 1;
            break;
        }
        next_idx = (next_idx + 1) % MAX_PROCESSES;
    }

    if (!found) {
        next_idx = current_process_idx;
        processes[next_idx].state = PROCESS_RUNNING;
    }

    current_process_idx = next_idx;
    processes[current_process_idx].state = PROCESS_RUNNING;

    return processes[current_process_idx].esp;
}

void process_sleep(u32 ticks) {
    if (current_process_idx == -1) return;
    processes[current_process_idx].state = PROCESS_SLEEPING;
    processes[current_process_idx].sleep_ticks = get_ticks() + ticks;
    while (processes[current_process_idx].state == PROCESS_SLEEPING) {
        __asm__ __volatile__("sti; hlt");
    }
}

void process_exit() {
    if (current_process_idx == -1) return;
    processes[current_process_idx].state = PROCESS_DEAD;
    process_count--;
    // Memory leak here since we don't safely kfree the stack while running on it.
    // In a real OS, a cleanup thread handles dead process stacks.
    while(1) {
         __asm__ __volatile__("int $0x20");
    }
}

u32 get_current_process_id() {
    if (current_process_idx == -1) return 0;
    return processes[current_process_idx].id;
}
