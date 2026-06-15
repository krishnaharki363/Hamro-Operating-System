#ifndef MEM_H
#define MEM_H

#include "type.h"

#define HEAP_START 0x20000
#define HEAP_END   0x60000

// Memory block header
typedef struct block {
    u32 size;            // Size of data section
    int free;            // 1 if block is free, 0 if allocated
    struct block *next;  // Next block in list
} block_t;

void init_mem();
void *kmalloc(u32 size);
void kfree(void *ptr);

u32 get_heap_total();
u32 get_heap_used();
u32 get_heap_free();
int get_heap_block_count();

#endif
