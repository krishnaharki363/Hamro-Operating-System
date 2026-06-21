#include "../include/mouse.h"
#include "../include/ports.h"
#include "../include/isr.h"
#include "../include/vga.h"

u8 mouse_cycle = 0;
u8 mouse_byte[4];
u8 mouse_has_wheel = 0;
int mouse_x = 40; // Center of 80x25 screen
int mouse_y = 12;

// Wait for mouse to be ready to read or write
void mouse_wait(u8 a_type) {
    u32 _time_out = 100000;
    if (a_type == 0) {
        while (_time_out--) {
            if ((port_byte_in(0x64) & 1) == 1) {
                return;
            }
        }
    } else {
        while (_time_out--) {
            if ((port_byte_in(0x64) & 2) == 0) {
                return;
            }
        }
    }
}

void mouse_write(u8 a_write) {
    mouse_wait(1);
    port_byte_out(0x64, 0xD4);
    mouse_wait(1);
    port_byte_out(0x60, a_write);
}

u8 mouse_read() {
    mouse_wait(0);
    return port_byte_in(0x60);
}

u32 mouse_callback(registers_t *regs) {
    u8 status = port_byte_in(0x64);
    if (!(status & 0x20)) return (u32)regs; // Not a mouse interrupt

    mouse_byte[mouse_cycle++] = port_byte_in(0x60);

    if (mouse_cycle == (mouse_has_wheel ? 4 : 3)) {
        mouse_cycle = 0;
        
        char *vid = (char*)0xb8000;
        
        // Remove old cursor highlight (restore original color)
        int old_offset = 2 * (mouse_y * 80 + mouse_x);
        u8 cur_attr = vid[old_offset + 1];
        vid[old_offset + 1] = cur_attr & 0x0F; // Remove background color
        
        // Extract X/Y
        int x_mov = mouse_byte[1] - ((mouse_byte[0] << 4) & 0x100);
        int y_mov = mouse_byte[2] - ((mouse_byte[0] << 3) & 0x100);

        // Adjust sensitivity
        mouse_x += x_mov / 2;
        mouse_y -= y_mov / 2;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x > 79) mouse_x = 79;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y > 24) mouse_y = 24;
        
        // Handle scroll wheel if present
        if (mouse_has_wheel) {
            char z_mov = (char)mouse_byte[3];
            if (z_mov > 0) {
                // Scroll wheel down -> scroll view down (towards live screen)
                vga_scroll_down();
            } else if (z_mov < 0) {
                // Scroll wheel up -> scroll view up (into history)
                vga_scroll_up();
            }
        }

        // Draw new cursor (add gray background)
        int new_offset = 2 * (mouse_y * 80 + mouse_x);
        u8 new_attr = vid[new_offset + 1];
        vid[new_offset + 1] = (0x70) | (new_attr & 0x0F);
    }

    return (u32)regs;
}

void init_mouse() {
    u8 _status;

    // Enable auxiliary mouse device
    mouse_wait(1);
    port_byte_out(0x64, 0xA8);

    // Enable interrupts
    mouse_wait(1);
    port_byte_out(0x64, 0x20);
    mouse_wait(0);
    _status = (port_byte_in(0x60) | 2);
    mouse_wait(1);
    port_byte_out(0x64, 0x60);
    mouse_wait(1);
    port_byte_out(0x60, _status);

    // Tell the mouse to use default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge

    // Initialize IntelliMouse (Scroll Wheel)
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();

    mouse_write(0xF2); // Get device ID
    mouse_read(); // Acknowledge
    u8 device_id = mouse_read(); // Read ID
    if (device_id == 3 || device_id == 4) {
        mouse_has_wheel = 1;
    }

    // Enable the mouse
    mouse_write(0xF4);
    mouse_read(); // Acknowledge

    register_interrupt_handler(IRQ12, mouse_callback);
}
