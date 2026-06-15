#include "timer.h"
#include "ports.h"
#include "vga.h"

static u32 tick_count = 0;

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;
}

void init_timer(u32 freq) {
    register_interrupt_handler(IRQ0, timer_callback);

    // PIT hardware clock runs at 1193180 Hz
    u32 divisor = 1193180 / freq;

    // Send the command byte (0x36) to command port 0x43
    // 0x36 = 00110110b -> Channel 0, access lobyte/hibyte, Mode 3 (Square Wave), Binary
    port_byte_out(0x43, 0x36);

    // Divisor must be sent byte-by-byte to channel 0 data port 0x40
    u8 l = (u8)(divisor & 0xFF);
    u8 h = (u8)((divisor >> 8) & 0xFF);

    port_byte_out(0x40, l);
    port_byte_out(0x40, h);
}

u32 get_ticks() {
    return tick_count;
}

void sleep(u32 ticks) {
    u32 end_tick = tick_count + ticks;
    while (tick_count < end_tick) {
        // Sleep until next interrupt to save host CPU cycles in QEMU
        __asm__ __volatile__("hlt");
    }
}
