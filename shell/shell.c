#include "shell.h"
#include "keyboard.h"
#include "timer.h"
#include "mem.h"
#include "vga.h"

// Custom string and number helpers since we have no standard C library
static int strlen(char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static int strcmp(char *s1, char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

static void int_to_ascii(int n, char *str) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    
    // Reverse string
    int len = i;
    for (int j = 0; j < len/2; j++) {
        char temp = str[j];
        str[j] = str[len-j-1];
        str[len-j-1] = temp;
    }
}

static void hex_to_ascii(u32 n, char *str) {
    str[0] = '0';
    str[1] = 'x';
    int i;
    const char *hex_chars = "0123456789ABCDEF";
    for (i = 7; i >= 0; i--) {
        str[i+2] = hex_chars[n & 0xF];
        n >>= 4;
    }
    str[10] = '\0';
}

// PRNG LCG Implementation
static u32 next_rand = 12345;
static void seed_rand(u32 seed) {
    next_rand = seed;
}
static int rand() {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}

// Mock Filesystem representation
struct mock_file {
    char name[32];
    char content[300];
};

static struct mock_file files[] = {
    {"welcome.txt", "Welcome to Hamro Operating System!\nThis is a custom x86 OS built for learning low-level programming.\nType 'help' to see available shell commands."},
    {"about.txt", "Hamro OS v1.0\nKernel: Custom 32-bit Protected Mode Kernel\nTarget Arch: i386-elf\nFeatures: VGA graphics driver, memory allocator, PIT timer, IRQs, keyboard driver."},
    {"todo.txt", "1. Implement multi-tasking and process scheduler\n2. Add read/write support for ext2 file systems\n3. Create a window server and GUI interface\n4. Rule the digital world!"},
    {"hacker.txt", "CLASSIFIED DATABASE:\n[DECRYPTED BY GUEST]\nWe are anonymous. We are legion. Expect us.\nFLAG: HAMRO_OS_HACKED_BY_NEO_2026"}
};
static const int num_files = 4;

// Shell command splitter
static void split_command(char *input, char *cmd, char *arg) {
    int i = 0;
    // Skip leading spaces
    while (input[i] == ' ') i++;
    
    int cmd_len = 0;
    while (input[i] != '\0' && input[i] != ' ') {
        if (cmd_len < 127) {
            cmd[cmd_len++] = input[i];
        }
        i++;
    }
    cmd[cmd_len] = '\0';
    
    // Skip spaces between cmd and arg
    while (input[i] == ' ') i++;
    
    int arg_len = 0;
    while (input[i] != '\0') {
        if (arg_len < 127) {
            arg[arg_len++] = input[i];
        }
        i++;
    }
    arg[arg_len] = '\0';
}

// Print customized prompt
static void print_prompt() {
    kprint_color("\nneo@hamro_os", 0x0A); // Light Green
    kprint_color(":", 0x0F);            // White
    kprint_color("~", 0x0B);            // Light Cyan
    kprint_color("$ ", 0x0F);           // White
}

// Neofetch command implementation
static void run_neofetch() {
    char num_buf[32];
    u32 secs = get_ticks() / 100;
    
    kprint_color("   ________      ", 0x0B); // Cyan logo
    kprint_color("neo@hamro-os\n", 0x0A); // Green user@host
    
    kprint_color("  /  ____  \\     ", 0x0B);
    kprint_color("------------\n", 0x0F); // White separator
    
    kprint_color(" |  /    \\  |    ", 0x0B);
    kprint_color("OS: ", 0x0E); // Yellow label
    kprint_color("Hamro Operating System v1.0\n", 0x0F);
    
    kprint_color(" | |      | |    ", 0x0B);
    kprint_color("Kernel: ", 0x0E);
    kprint_color("x86-32 Protected Mode Kernel\n", 0x0F);
    
    kprint_color(" |  \\____/  |    ", 0x0B);
    kprint_color("Uptime: ", 0x0E);
    int_to_ascii(secs, num_buf);
    kprint_color(num_buf, 0x0F);
    kprint_color("s\n", 0x0F);
    
    kprint_color("  \\________/     ", 0x0B);
    kprint_color("Memory: ", 0x0E);
    int_to_ascii(get_heap_used(), num_buf);
    kprint_color(num_buf, 0x0F);
    kprint_color(" / ", 0x0F);
    int_to_ascii(get_heap_total(), num_buf);
    kprint_color(num_buf, 0x0F);
    kprint_color(" bytes\n", 0x0F);
    
    kprint_color("    /    \\       ", 0x0B);
    kprint_color("VGA Mode: ", 0x0E);
    kprint_color("80x25 16-color Text Mode\n", 0x0F);
    
    kprint_color("   /______\\      ", 0x0B);
    kprint_color("Shell: ", 0x0E);
    kprint_color("hacker-sh v1.1\n", 0x0F);
    
    kprint_color("  /________\\     ", 0x0B);
    kprint_color("Theme: ", 0x0E);
    kprint_color("Neon Green [Hacker Vibes]\n", 0x0A);
    
    kprint_color("                 ", 0x0B);
    // Print color palette chips like neofetch
    kprint_color("[ ] ", 0x00);
    kprint_color("██ ", 0x04); // Red chip
    kprint_color("██ ", 0x02); // Green chip
    kprint_color("██ ", 0x0E); // Yellow/Brown chip
    kprint_color("██ ", 0x01); // Blue chip
    kprint_color("██ ", 0x05); // Magenta chip
    kprint_color("██ ", 0x03); // Cyan chip
    kprint_color("██\n", 0x0F); // White chip
}

// Matrix digital rain animation
static void run_matrix() {
    clear_screen();
    seed_rand(get_ticks());
    
    int y_pos[80];
    int length[80];
    int speed[80];
    int count[80];
    
    for (int i = 0; i < 80; i++) {
        y_pos[i] = -1; // -1 means inactive
        length[i] = 0;
        speed[i] = 0;
        count[i] = 0;
    }
    
    char *vidmem = (char *)VIDEO_ADDRESS;
    
    while (1) {
        // Exit matrix on any keypress
        char c = get_last_char();
        if (c != 0) {
            clear_last_char();
            clear_screen();
            return;
        }
        
        for (int col = 0; col < 80; col++) {
            if (y_pos[col] == -1) {
                // 5% chance of starting a new drop
                if (rand() % 100 < 5) {
                    y_pos[col] = 0;
                    length[col] = rand() % 12 + 5; 
                    speed[col] = rand() % 3 + 1;   
                    count[col] = 0;
                }
            } else {
                count[col]++;
                if (count[col] >= speed[col]) {
                    count[col] = 0;
                    
                    int head_y = y_pos[col];
                    if (head_y < 25) {
                        int offset = 2 * (head_y * 80 + col);
                        char rand_char = (char)(rand() % 93 + 33); // characters 33 to 126
                        vidmem[offset] = rand_char;
                        vidmem[offset + 1] = 0x0F; // White highlight at tip
                    }
                    
                    if (head_y - 1 >= 0 && head_y - 1 < 25) {
                        int offset = 2 * ((head_y - 1) * 80 + col);
                        vidmem[offset + 1] = 0x0A; // Bright Green
                    }
                    if (head_y - 3 >= 0 && head_y - 3 < 25) {
                        int offset = 2 * ((head_y - 3) * 80 + col);
                        vidmem[offset + 1] = 0x02; // Dark Green
                    }
                    
                    // Erase trailing character
                    int tail_y = head_y - length[col];
                    if (tail_y >= 0 && tail_y < 25) {
                        int offset = 2 * (tail_y * 80 + col);
                        vidmem[offset] = ' ';
                        vidmem[offset + 1] = 0x0A;
                    }
                    
                    y_pos[col]++;
                    if (y_pos[col] - length[col] >= 25) {
                        y_pos[col] = -1;
                    }
                }
            }
        }
        
        sleep(4); // Sleep 4 ticks (40ms)
    }
}

// Fake system hack simulation
static void run_hack() {
    clear_screen();
    seed_rand(get_ticks());
    
    kprint_color("[!] WARNING: SECURE NETWORKS DETECTED\n", 0x0C); // Red
    sleep(80);
    kprint_color("[*] Initiating connection to remote mainframe...\n", 0x0E); // Yellow
    sleep(80);
    
    for (int i = 0; i < 35; i++) {
        if (get_last_char() != 0) {
            clear_last_char();
            clear_screen();
            return;
        }
        
        char hex_buf[32];
        kprint_color("  Attacking memory sector: ", 0x0A);
        hex_to_ascii((u32)(rand() % 65536 + 0xD0000), hex_buf);
        kprint_color(hex_buf, 0x0B); // Cyan
        kprint_color("  Status: ", 0x0A);
        
        if (rand() % 6 == 0) {
            kprint_color("[ENCRYPTED]\n", 0x0C); // Red
        } else {
            kprint_color("[BYPASSED]\n", 0x0A); // Green
        }
        sleep(8 + (rand() % 12));
    }
    
    kprint_color("\n[*] Mainframe bypass established. Downloading databanks...\n", 0x0E);
    sleep(100);
    
    char progress_bar[] = "Decrypting: [░░░░░░░░░░░░░░░░░░░░] 000%";
    kprint_color("\n", 0x0A);
    
    for (int percentage = 0; percentage <= 100; percentage += 5) {
        if (get_last_char() != 0) {
            clear_last_char();
            clear_screen();
            return;
        }
        
        int solid_blocks = percentage / 5;
        for (int b = 0; b < 20; b++) {
            if (b < solid_blocks) {
                progress_bar[13 + b] = (char)219; // █
            } else {
                progress_bar[13 + b] = (char)176; // ░
            }
        }
        
        int hundreds = percentage / 100;
        int tens = (percentage % 100) / 10;
        int ones = percentage % 10;
        progress_bar[35] = (char)(hundreds + '0');
        progress_bar[36] = (char)(tens + '0');
        progress_bar[37] = (char)(ones + '0');
        
        if (percentage > 0) {
            for (int bs = 0; bs < 39; bs++) {
                kprint("\b");
            }
        }
        kprint_color(progress_bar, 0x0E); // Yellow
        sleep(12);
    }
    
    kprint_color("\n\n[+] DECRYPTION COMPLETE!\n", 0x0A);
    sleep(80);
    kprint_color("[+] SYSTEM OVERRIDDEN. KEYLOGGERS ACTIVE.\n", 0x0B);
    sleep(120);
    kprint_color("\nPress any key to return to Hamro OS...", 0x0F);
    
    while (get_last_char() == 0) {
        __asm__ __volatile__("hlt");
    }
    clear_last_char();
    clear_screen();
}

static void execute_command(char *input) {
    char cmd[128];
    char arg[128];
    split_command(input, cmd, arg);
    
    if (strcmp(cmd, "help") == 0) {
        kprint_color("Available commands:\n", 0x0E); // Yellow header
        kprint_color("  help     ", 0x0B); kprint("- Display this help menu\n");
        kprint_color("  clear    ", 0x0B); kprint("- Clear the screen\n");
        kprint_color("  uptime   ", 0x0B); kprint("- Display system uptime (timer ticks)\n");
        kprint_color("  sleep    ", 0x0B); kprint("- Pause execution for 2 seconds\n");
        kprint_color("  meminfo  ", 0x0B); kprint("- Display heap memory management statistics\n");
        kprint_color("  memtest  ", 0x0B); kprint("- Run memory allocator stress tests\n");
        kprint_color("  ls       ", 0x0B); kprint("- List available files on current disk\n");
        kprint_color("  cat <f>  ", 0x0B); kprint("- Output mock file contents on screen\n");
        kprint_color("  neofetch ", 0x0B); kprint("- Print system metadata and logo\n");
        kprint_color("  matrix   ", 0x0B); kprint("- Launch digital code rain animation\n");
        kprint_color("  hack     ", 0x0B); kprint("- Simulate mainframe hacker attack\n");
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (strcmp(cmd, "uptime") == 0) {
        u32 ticks = get_ticks();
        u32 secs = ticks / 100;
        char num_buf[32];
        
        kprint("System Uptime: ");
        int_to_ascii(secs, num_buf);
        kprint_color(num_buf, 0x0E); // Yellow highlights
        kprint(" seconds (");
        int_to_ascii(ticks, num_buf);
        kprint_color(num_buf, 0x0E);
        kprint(" ticks)\n");
    } else if (strcmp(cmd, "sleep") == 0) {
        kprint("Sleeping for 2 seconds (200 ticks)... ");
        sleep(200);
        kprint_color("Awake!\n", 0x0A);
    } else if (strcmp(cmd, "meminfo") == 0) {
        char num_buf[32];
        
        kprint_color("Heap Memory Status:\n", 0x0E);
        
        kprint("  Total Heap Size  : ");
        int_to_ascii(get_heap_total(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Used (w/ headers): ");
        int_to_ascii(get_heap_used(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Free Usable Size : ");
        int_to_ascii(get_heap_free(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Total Blocks     : ");
        int_to_ascii(get_heap_block_count(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint("\n");
    } else if (strcmp(cmd, "memtest") == 0) {
        char ptr_buf[32];
        
        kprint_color("Starting Memory Allocator Tests...\n", 0x0E);
        
        kprint("1. Allocating Block A (16 bytes)... ");
        void *a = kmalloc(16);
        if (a) {
            kprint_color("Allocated at ", 0x0A);
            hex_to_ascii((u32)a, ptr_buf);
            kprint_color(ptr_buf, 0x0B);
            kprint("\n");
        } else {
            kprint_color("Failed!\n", 0x0C);
        }
        
        kprint("2. Allocating Block B (64 bytes)... ");
        void *b = kmalloc(64);
        if (b) {
            kprint_color("Allocated at ", 0x0A);
            hex_to_ascii((u32)b, ptr_buf);
            kprint_color(ptr_buf, 0x0B);
            kprint("\n");
        } else {
            kprint_color("Failed!\n", 0x0C);
        }
        
        kprint("3. Freeing Block A... ");
        kfree(a);
        kprint_color("Freed.\n", 0x0A);
        
        kprint("4. Allocating Block C (8 bytes) - Should reuse A's block: ");
        void *c = kmalloc(8);
        if (c) {
            kprint_color("Allocated at ", 0x0A);
            hex_to_ascii((u32)c, ptr_buf);
            kprint_color(ptr_buf, 0x0B);
            kprint("\n");
        } else {
            kprint_color("Failed!\n", 0x0C);
        }
        
        kprint("5. Freeing Block B and C... ");
        kfree(b);
        kfree(c);
        kprint_color("Freed.\n", 0x0A);
        
        kprint_color("Memory allocator tests successfully finished.\n", 0x0A);
    } else if (strcmp(cmd, "ls") == 0) {
        kprint_color("Directory listing for / (RAM Disk):\n", 0x0E);
        for (int i = 0; i < num_files; i++) {
            kprint_color("  -rwxr-xr-x   1 root     root       ", 0x02); // Dim Green perm
            
            // Format size
            char size_str[16];
            int_to_ascii(strlen(files[i].content), size_str);
            
            // Align sizes nicely (simple padding)
            int spaces = 5 - strlen(size_str);
            for (int s = 0; s < spaces; s++) kprint(" ");
            kprint_color(size_str, 0x0A);
            kprint_color(" bytes  ", 0x02);
            
            kprint_color(files[i].name, 0x0B); // Cyan file name
            kprint("\n");
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (strlen(arg) == 0) {
            kprint_color("Usage: cat <filename>\n", 0x0C); // Red error
        } else {
            int found = 0;
            for (int i = 0; i < num_files; i++) {
                if (strcmp(files[i].name, arg) == 0) {
                    kprint_color(files[i].content, 0x0F); // White text output
                    kprint("\n");
                    found = 1;
                    break;
                }
            }
            if (!found) {
                kprint_color("cat: file not found: '", 0x0C);
                kprint_color(arg, 0x0C);
                kprint_color("'\n", 0x0C);
            }
        }
    } else if (strcmp(cmd, "neofetch") == 0) {
        run_neofetch();
    } else if (strcmp(cmd, "matrix") == 0) {
        run_matrix();
    } else if (strcmp(cmd, "hack") == 0) {
        run_hack();
    } else if (strlen(cmd) > 0) {
        kprint_color("Unknown command: '", 0x0C);
        kprint_color(cmd, 0x0C);
        kprint_color("'. Type 'help' for commands.\n", 0x0C);
    }
}

void run_shell() {
    char cmd_buffer[256];
    int cmd_index = 0;
    
    print_prompt();
    
    while (1) {
        char c = get_last_char();
        if (c != 0) {
            clear_last_char();
            
            if (c == '\n') {
                kprint("\n");
                cmd_buffer[cmd_index] = '\0';
                
                execute_command(cmd_buffer);
                
                cmd_index = 0;
                print_prompt();
            } else if (c == '\b') {
                if (cmd_index > 0) {
                    cmd_index--;
                    kprint("\b");
                }
            } else {
                if (cmd_index < 254) {
                    cmd_buffer[cmd_index++] = c;
                    char char_str[2];
                    char_str[0] = c;
                    char_str[1] = '\0';
                    kprint(char_str);
                }
            }
        }
        __asm__ __volatile__("hlt");
    }
}
