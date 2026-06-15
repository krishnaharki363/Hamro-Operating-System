#include "../include/vga.h"
#include "../include/isr.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/mem.h"
#include "../include/shell.h"

void kernel_main() {
    clear_screen();
    
    // Colorful ASCII Banner for "Hamro OS"
    kprint_color("==================================================================\n", 0x0B); // Cyan
    kprint_color("  _    _                               ____   ____  \n", 0x0A); // Light Green
    kprint_color(" | |  | | __ _ _ __ ___  _ __ ___     / __ \\ / ___| \n", 0x0A);
    kprint_color(" | |__| |/ _` | '_ ` _ \\| '__/ _ \\   | |  | |\\___ \\ \n", 0x0A);
    kprint_color(" |  __  | (_| | | | | | | | | (_) |  | |__| | ___) |\n", 0x0A);
    kprint_color(" |_|  |_|\\__,_|_| |_| |_|_|  \\___/    \\____/ |____/ \n", 0x0A);
    kprint_color("                                                    \n", 0x0A);
    kprint_color("               -- HAMRO OPERATING SYSTEM v1.0 --    \n", 0x0E); // Yellow
    kprint_color("==================================================================\n\n", 0x0B); // Cyan
    
    kprint_color("Initializing OS components...\n", 0x0F); // White
    
    // 1. Initialize Heap Memory Manager
    kprint_color("-> Memory Manager... ", 0x0F);
    init_mem();
    kprint_color("[OK]\n", 0x0A); // Light Green
    
    // 2. Install Interrupts Descriptor Table (IDT) and ISRs
    kprint_color("-> Interrupt Descriptors Table (IDT)... ", 0x0F);
    isr_install();
    kprint_color("[OK]\n", 0x0A);
    
    // 3. Initialize PIT Timer (100Hz frequency)
    kprint_color("-> PIT Timer (100Hz)... ", 0x0F);
    init_timer(100);
    kprint_color("[OK]\n", 0x0A);
    
    // 4. Initialize Keyboard Driver
    kprint_color("-> Keyboard Driver... ", 0x0F);
    init_keyboard();
    kprint_color("[OK]\n", 0x0A);
    
    // 5. Enable hardware interrupts
    kprint_color("Enabling Interrupts... ", 0x0F);
    __asm__ __volatile__("sti");
    kprint_color("[OK]\n", 0x0A);
    
    kprint_color("\nSystem ready. Launching interactive shell...\n", 0x0B); // Light Cyan
    
    // 6. Transfer control to the interactive shell
    run_shell();
}

