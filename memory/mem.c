#include "mem.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

static block_t *heap_start_block = (block_t*)HEAP_START;

void init_mem() {
    heap_start_block->size = (HEAP_END - HEAP_START) - sizeof(block_t);
    heap_start_block->free = 1;
    heap_start_block->next = NULL;
}

void *kmalloc(u32 size) {
    if (size == 0) return NULL;
    
    // Align size to 4-byte boundary
    size = (size + 3) & ~3;

    block_t *curr = heap_start_block;
    while (curr != NULL) {
        if (curr->free && curr->size >= size) {
            // Check if we can split this block.
            // Split requires space for the requested size, a block header, and at least 4 bytes of data.
            u32 required_space = size + sizeof(block_t) + 4;
            if (curr->size >= required_space) {
                // Perform block split
                block_t *new_block = (block_t*)((char*)curr + sizeof(block_t) + size);
                new_block->size = curr->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
            curr->free = 0;
            return (void*)((char*)curr + sizeof(block_t));
        }
        curr = curr->next;
    }
    return NULL; // Out of Memory
}

void kfree(void *ptr) {
    if (ptr == NULL) return;

    block_t *block = (block_t*)((char*)ptr - sizeof(block_t));
    block->free = 1;

    // Coalesce free blocks to prevent fragmentation
    block_t *curr = heap_start_block;
    while (curr != NULL && curr->next != NULL) {
        if (curr->free && curr->next->free) {
            curr->size = curr->size + sizeof(block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

u32 get_heap_total() {
    return HEAP_END - HEAP_START;
}

u32 get_heap_used() {
    u32 used = 0;
    block_t *curr = heap_start_block;
    while (curr != NULL) {
        if (!curr->free) {
            used += curr->size + sizeof(block_t);
        }
        curr = curr->next;
    }
    return used;
}

u32 get_heap_free() {
    u32 free_space = 0;
    block_t *curr = heap_start_block;
    while (curr != NULL) {
        if (curr->free) {
            free_space += curr->size;
        }
        curr = curr->next;
    }
    return free_space;
}

int get_heap_block_count() {
    int count = 0;
    block_t *curr = heap_start_block;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

void *memcpy(void *dest, const void *src, u32 n) {
    char *d = dest;
    const char *s = src;
    for (u32 i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

