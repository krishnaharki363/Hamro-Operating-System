#ifndef IDT_H
#define IDT_H

#include "type.h"

// Segment selectors
#define KERNEL_CS 0x08

// A struct describing an interrupt gate
typedef struct {
    u16 low_offset; // Lower 16 bits of handler address
    u16 sel;        // Kernel segment selector
    u8 always0;     // This must always be zero
    /* First byte
     * Bit 7: "Interrupt is present"
     * Bits 6-5: Privilege level of caller (0=kernel..3=user)
     * Bit 4: Set to 0 for interrupt gates
     * Bits 3-0: bits 1110 = decimal 14 = "32 bit interrupt gate" */
    u8 flags;
    u16 high_offset; // Higher 16 bits of handler address
} __attribute__((packed)) idt_gate_t;

// A struct describing the pointer to the array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
typedef struct {
    u16 limit;
    u32 base; // The address of the first element in our idt_gate_t array
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256

void set_idt_gate(int n, u32 handler);
void set_idt();

#endif
