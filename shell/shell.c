#include "shell.h"
#include "keyboard.h"
#include "timer.h"
#include "mem.h"
#include "vga.h"

// Helper functions for string manipulation since we don't have standard C library
static int strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static int strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}


static void strcpy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
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

// Shell Command Parsing Helpers
static void split_command(char *input, char *cmd, char *arg) {
    int i = 0;
    while (input[i] == ' ') i++;
    
    int cmd_len = 0;
    while (input[i] != '\0' && input[i] != ' ') {
        if (cmd_len < 127) cmd[cmd_len++] = input[i];
        i++;
    }
    cmd[cmd_len] = '\0';
    
    while (input[i] == ' ') i++;
    
    int arg_len = 0;
    while (input[i] != '\0') {
        if (arg_len < 127) arg[arg_len++] = input[i];
        i++;
    }
    arg[arg_len] = '\0';
}

static void split_two_args(char *input, char *arg1, char *arg2) {
    int i = 0;
    while (input[i] == ' ') i++;
    
    int len1 = 0;
    while (input[i] != '\0' && input[i] != ' ') {
        if (len1 < 63) arg1[len1++] = input[i];
        i++;
    }
    arg1[len1] = '\0';
    
    while (input[i] == ' ') i++;
    
    int len2 = 0;
    while (input[i] != '\0') {
        if (len2 < 63) arg2[len2++] = input[i];
        i++;
    }
    arg2[len2] = '\0';
}

// RAM Filesystem Structures & Global State
#define MAX_FILES 32
#define FILE_TYPE_REG 0
#define FILE_TYPE_DIR 1

struct mock_inode {
    char name[64];      // Absolute path to node
    int type;           // FILE_TYPE_REG or FILE_TYPE_DIR
    char content[512];  // In-memory file content
    int size;           // File size in bytes
    int active;         // 1 if active, 0 if deleted
};

static struct mock_inode fs[MAX_FILES];
static int fs_count = 0;
static char cwd[128] = "/";

// Path Resolution and Normalization (Stack-Based Resolver)
static void to_absolute_path(char *input_path, char *out_path) {
    char temp[128];
    if (input_path[0] == '/') {
        strcpy(temp, input_path);
    } else {
        strcpy(temp, cwd);
        if (strcmp(cwd, "/") != 0) {
            int len = strlen(temp);
            temp[len] = '/';
            temp[len+1] = '\0';
        }
        strcpy(temp + strlen(temp), input_path);
    }
    
    // Normalize path by removing "//" and processing "." / ".."
    char clean_temp[128];
    int ct = 0;
    int i = 0;
    while (temp[i] != '\0') {
        if (temp[i] == '/' && temp[i+1] == '/') {
            i++;
        } else {
            clean_temp[ct++] = temp[i++];
        }
    }
    clean_temp[ct] = '\0';
    
    // Tokenize parts and process via folder stack
    char stack[8][32];
    int stack_top = 0;
    int idx = 0;
    int len = strlen(clean_temp);
    
    while (idx < len) {
        if (clean_temp[idx] == '/') {
            idx++;
            continue;
        }
        
        char part[32];
        int p_idx = 0;
        while (clean_temp[idx] != '\0' && clean_temp[idx] != '/') {
            if (p_idx < 31) {
                part[p_idx++] = clean_temp[idx];
            }
            idx++;
        }
        part[p_idx] = '\0';
        
        if (strcmp(part, ".") == 0) {
            // Ignore current directory dot
        } else if (strcmp(part, "..") == 0) {
            if (stack_top > 0) {
                stack_top--;
            }
        } else if (strlen(part) > 0) {
            if (stack_top < 8) {
                strcpy(stack[stack_top], part);
                stack_top++;
            }
        }
    }
    
    // Reconstruct final path from stack
    strcpy(out_path, "/");
    for (int k = 0; k < stack_top; k++) {
        if (k > 0) {
            int flen = strlen(out_path);
            out_path[flen] = '/';
            out_path[flen+1] = '\0';
        }
        int flen = strlen(out_path);
        strcpy(out_path + flen, stack[k]);
    }
    
    // Enforce leading slash
    if (out_path[0] != '/') {
        char shifted[128];
        strcpy(shifted, "/");
        strcpy(shifted + 1, out_path);
        strcpy(out_path, shifted);
    }
}

// Verify parent directory exists and is of type DIR
static int parent_exists(const char *path) {
    int last_slash = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') last_slash = i;
    }
    
    if (last_slash == 0) return 1; // Parent is root directory "/"
    
    char parent[128];
    for (int i = 0; i < last_slash; i++) {
        parent[i] = path[i];
    }
    parent[last_slash] = '\0';
    
    for (int i = 0; i < fs_count; i++) {
        if (fs[i].active && strcmp(fs[i].name, parent) == 0) {
            return (fs[i].type == FILE_TYPE_DIR);
        }
    }
    return 0;
}

// Check if a path sits directly inside the directory (non-recursive)
static int is_in_directory(const char *file_path, const char *dir_path) {
    int dir_len = strlen(dir_path);
    
    if (strcmp(dir_path, "/") == 0) {
        if (file_path[0] != '/') return 0;
        int i = 1;
        while (file_path[i] != '\0') {
            if (file_path[i] == '/') return 0;
            i++;
        }
        return 1;
    }
    
    if (strlen(file_path) <= dir_len) return 0;
    
    for (int i = 0; i < dir_len; i++) {
        if (file_path[i] != dir_path[i]) return 0;
    }
    
    if (file_path[dir_len] != '/') return 0;
    
    int i = dir_len + 1;
    while (file_path[i] != '\0') {
        if (file_path[i] == '/') return 0;
        i++;
    }
    return 1;
}

// RAM FS Initializer
static void init_fs() {
    // 1. welcome.txt
    strcpy(fs[0].name, "/welcome.txt");
    fs[0].type = FILE_TYPE_REG;
    strcpy(fs[0].content, "Welcome to Hamro Operating System!\nThis is a custom x86 OS built for learning low-level programming.\nType 'help' to see available shell commands.");
    fs[0].size = strlen(fs[0].content);
    fs[0].active = 1;

    // 2. about.txt
    strcpy(fs[1].name, "/about.txt");
    fs[1].type = FILE_TYPE_REG;
    strcpy(fs[1].content, "Hamro OS v1.0\nKernel: Custom 32-bit Protected Mode Kernel\nTarget Arch: i386-elf\nFeatures: VGA graphics driver, memory allocator, PIT timer, IRQs, keyboard driver.");
    fs[1].size = strlen(fs[1].content);
    fs[1].active = 1;

    // 3. todo.txt
    strcpy(fs[2].name, "/todo.txt");
    fs[2].type = FILE_TYPE_REG;
    strcpy(fs[2].content, "1. Implement multi-tasking and process scheduler\n2. Add read/write support for ext2 file systems\n3. Create a window server and GUI interface\n4. Rule the digital world!");
    fs[2].size = strlen(fs[2].content);
    fs[2].active = 1;

    // 4. hacker.txt
    strcpy(fs[3].name, "/hacker.txt");
    fs[3].type = FILE_TYPE_REG;
    strcpy(fs[3].content, "CLASSIFIED DATABASE:\n[DECRYPTED BY GUEST]\nWe are anonymous. We are legion. Expect us.\nFLAG: HAMRO_OS_HACKED_BY_NEO_2026");
    fs[3].size = strlen(fs[3].content);
    fs[3].active = 1;

    // 5. documents directory
    strcpy(fs[4].name, "/documents");
    fs[4].type = FILE_TYPE_DIR;
    fs[4].size = 0;
    fs[4].active = 1;

    // 6. notes.txt inside /documents
    strcpy(fs[5].name, "/documents/notes.txt");
    fs[5].type = FILE_TYPE_REG;
    strcpy(fs[5].content, "Just some notes: clean the bedroom, write kernel code, enjoy life.");
    fs[5].size = strlen(fs[5].content);
    fs[5].active = 1;

    fs_count = 6;
}

// Print customized prompt with current working directory (CWD)
static void print_prompt() {
    kprint_color("\nneo@hamro_os", 0x0A); // Light Green
    kprint_color(":", 0x0F);            // White
    kprint_color(cwd, 0x0B);            // Light Cyan CWD
    kprint_color("$ ", 0x0F);           // White
}

// Neofetch command
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
    kprint_color("hacker-sh v1.2\n", 0x0F);
    
    kprint_color("  /________\\     ", 0x0B);
    kprint_color("Theme: ", 0x0E);
    kprint_color("Neon Green [Hacker Vibes]\n", 0x0A);
    
    kprint_color("                 ", 0x0B);
    kprint_color("[ ] ", 0x00);
    kprint_color("██ ", 0x04); // Red
    kprint_color("██ ", 0x02); // Green
    kprint_color("██ ", 0x0E); // Yellow
    kprint_color("██ ", 0x01); // Blue
    kprint_color("██ ", 0x05); // Magenta
    kprint_color("██ ", 0x03); // Cyan
    kprint_color("██\n", 0x0F); // White
}

// Matrix code rain screen saver
static void run_matrix() {
    clear_screen();
    seed_rand(get_ticks());
    
    int y_pos[80];
    int length[80];
    int speed[80];
    int count[80];
    
    for (int i = 0; i < 80; i++) {
        y_pos[i] = -1;
        length[i] = 0;
        speed[i] = 0;
        count[i] = 0;
    }
    
    char *vidmem = (char *)VIDEO_ADDRESS;
    
    while (1) {
        char c = get_last_char();
        if (c != 0) {
            clear_last_char();
            clear_screen();
            return;
        }
        
        for (int col = 0; col < 80; col++) {
            if (y_pos[col] == -1) {
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
                        char rand_char = (char)(rand() % 93 + 33);
                        vidmem[offset] = rand_char;
                        vidmem[offset + 1] = 0x0F; // White glows
                    }
                    
                    if (head_y - 1 >= 0 && head_y - 1 < 25) {
                        int offset = 2 * ((head_y - 1) * 80 + col);
                        vidmem[offset + 1] = 0x0A; // Light Green
                    }
                    if (head_y - 3 >= 0 && head_y - 3 < 25) {
                        int offset = 2 * ((head_y - 3) * 80 + col);
                        vidmem[offset + 1] = 0x02; // Dark Green
                    }
                    
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
        sleep(4);
    }
}

// Hacker intrusion simulation
static void run_hack() {
    clear_screen();
    seed_rand(get_ticks());
    
    kprint_color("[!] WARNING: SECURE PORTALS DETECTED\n", 0x0C);
    sleep(80);
    kprint_color("[*] Opening connection tunnel to host database...\n", 0x0E);
    sleep(80);
    
    for (int i = 0; i < 35; i++) {
        if (get_last_char() != 0) {
            clear_last_char();
            clear_screen();
            return;
        }
        
        char hex_buf[32];
        kprint_color("  Cracking node address: ", 0x0A);
        hex_to_ascii((u32)(rand() % 65536 + 0xE0000), hex_buf);
        kprint_color(hex_buf, 0x0B);
        kprint_color("  Status: ", 0x0A);
        
        if (rand() % 6 == 0) {
            kprint_color("[ENCRYPTED]\n", 0x0C);
        } else {
            kprint_color("[BYPASSED]\n", 0x0A);
        }
        sleep(8 + (rand() % 12));
    }
    
    kprint_color("\n[*] Database firewall bypassed. decrpyting tables...\n", 0x0E);
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
                progress_bar[13 + b] = (char)219;
            } else {
                progress_bar[13 + b] = (char)176;
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
        kprint_color(progress_bar, 0x0E);
        sleep(12);
    }
    
    kprint_color("\n\n[+] DECRYPTION COMPLETE!\n", 0x0A);
    sleep(80);
    kprint_color("[+] SYSTEM hijacked. Access granted.\n", 0x0B);
    sleep(120);
    kprint_color("\nPress any key to return to Hamro OS...", 0x0F);
    
    while (get_last_char() == 0) {
        __asm__ __volatile__("hlt");
    }
    clear_last_char();
    clear_screen();
}

/* ===================================================================
 * SNAKE GAME  —  WASD to move, Q to quit
 * Play area: x 1..78, y 2..22  |  100Hz timer drives speed
 * =================================================================== */
#define SNAKE_MAX  100
#define SN_UP    0
#define SN_DOWN  1
#define SN_LEFT  2
#define SN_RIGHT 3

static void sn_vid(int col, int row, char c, u8 attr) {
    char *vid = (char *)0xB8000;
    int off = 2 * (row * 80 + col);
    vid[off] = c; vid[off + 1] = attr;
}

static void sn_numprint(int col, int row, int n, u8 attr) {
    char buf[16];
    int_to_ascii(n, buf);
    for (int i = 0; buf[i]; i++) sn_vid(col + i, row, buf[i], attr);
    sn_vid(col + strlen(buf), row, ' ', attr);  /* clear leftover digit */
}

static void run_snake() {
    clear_screen();
    seed_rand(get_ticks());

    /* Header bar */
    char *hdr = "  HAMRO SNAKE  |  WASD: Move  |  Q: Quit  |  Score: ";
    for (int i = 0; hdr[i] && i < 80; i++) sn_vid(i, 0, hdr[i], 0x3F);
    sn_numprint(53, 0, 0, 0x3F);

    /* CP437 double-line border */
    for (int x = 1; x < 79; x++) { sn_vid(x,  1, (char)205, 0x0B); sn_vid(x, 23, (char)205, 0x0B); }
    for (int y = 2; y <= 22;  y++) { sn_vid(0, y, (char)186, 0x0B); sn_vid(79, y, (char)186, 0x0B); }
    sn_vid( 0,  1, (char)201, 0x0B); sn_vid(79,  1, (char)187, 0x0B);
    sn_vid( 0, 23, (char)200, 0x0B); sn_vid(79, 23, (char)188, 0x0B);

    /* Snake state */
    int sx[SNAKE_MAX], sy[SNAKE_MAX];
    int slen = 5, dir = SN_RIGHT, ndir = SN_RIGHT;
    int score = 0, dead = 0;

    for (int i = 0; i < slen; i++) { sx[i] = 39 - i; sy[i] = 12; }
    sn_vid(sx[0], sy[0], '@', 0x0A);
    for (int i = 1; i < slen; i++) sn_vid(sx[i], sy[i], (char)219, 0x02);

    /* Initial food */
    int fx = 1 + rand() % 78;
    int fy = 2 + rand() % 21;
    sn_vid(fx, fy, (char)4, 0x0C);   /* CP437 diamond, red */

    u32 interval = 8;                 /* ticks between moves (100 Hz) */
    u32 last_t   = get_ticks();

    while (!dead) {
        /* Drain keyboard ring buffer */
        char c;
        while ((c = get_last_char()) != 0) {
            clear_last_char();
            if ((c=='w'||c=='W') && dir!=SN_DOWN)  ndir = SN_UP;
            if ((c=='s'||c=='S') && dir!=SN_UP)    ndir = SN_DOWN;
            if ((c=='a'||c=='A') && dir!=SN_RIGHT)  ndir = SN_LEFT;
            if ((c=='d'||c=='D') && dir!=SN_LEFT)   ndir = SN_RIGHT;
            if  (c=='q'||c=='Q') { dead = 2; }
        }
        if (dead) break;

        u32 now = get_ticks();
        if (now - last_t < interval) { __asm__ __volatile__("pause"); continue; }
        last_t = now;
        dir = ndir;

        /* Compute new head position */
        int nx = sx[0], ny = sy[0];
        if (dir == SN_RIGHT) nx++;
        else if (dir == SN_LEFT) nx--;
        else if (dir == SN_DOWN) ny++;
        else ny--;

        /* Wall collision */
        if (nx < 1 || nx > 78 || ny < 2 || ny > 22) { dead = 1; break; }

        /* Self collision (skip head at i=0) */
        for (int i = 1; i < slen && !dead; i++)
            if (sx[i] == nx && sy[i] == ny) dead = 1;
        if (dead) break;

        int ate = (nx == fx && ny == fy);

        /* Erase tail before shift (skip if growing) */
        if (!ate) sn_vid(sx[slen-1], sy[slen-1], ' ', 0x00);
        if (ate && slen < SNAKE_MAX) slen++;

        /* Shift body */
        for (int i = slen-1; i > 0; i--) { sx[i]=sx[i-1]; sy[i]=sy[i-1]; }
        sx[0]=nx; sy[0]=ny;

        /* Redraw head and second segment */
        sn_vid(sx[0], sy[0], '@', 0x0A);
        if (slen > 1) sn_vid(sx[1], sy[1], (char)219, 0x02);

        if (ate) {
            score++;
            sn_numprint(53, 0, score, 0x3F);
            if (score % 5 == 0 && interval > 2) interval--;  /* speed up */
            /* Spawn food not on snake body */
            int tries = 0;
            do {
                fx = 1 + rand() % 78;
                fy = 2 + rand() % 21;
                int ok = 1;
                for (int i = 0; i < slen; i++)
                    if (sx[i]==fx && sy[i]==fy) { ok=0; break; }
                if (ok || ++tries > 200) break;
            } while (1);
            sn_vid(fx, fy, (char)4, 0x0C);
        }
    }

    /* Game Over overlay */
    if (dead == 1) {
        char *go = "  GAME OVER!  Press any key to continue...  ";
        int c0 = (80 - 44) / 2;
        for (int i = 0; go[i]; i++) sn_vid(c0+i, 12, go[i], 0x4F);  /* white on red */
    }
    while (get_last_char() == 0) __asm__ __volatile__("pause");
    clear_last_char();
    clear_screen();
}

/* ===================================================================
 * CALCULATOR  —  recursive-descent parser
 * Supports: integers, +  -  *  /  ( )  and unary minus
 * Operator precedence: * / before + -
 * =================================================================== */
static const char *cl_str;   /* expression being parsed */
static int         cl_p;     /* current position        */

static void cl_skip(void) { while (cl_str[cl_p] == ' ') cl_p++; }
static int  cl_eval(void);   /* forward decl for parenthesis recursion */

static int cl_prim(void) {
    cl_skip();
    if (cl_str[cl_p] == '(') {
        cl_p++;
        int v = cl_eval(); cl_skip();
        if (cl_str[cl_p] == ')') cl_p++;
        return v;
    }
    int sign = 1;
    if      (cl_str[cl_p] == '-') { sign = -1; cl_p++; }
    else if (cl_str[cl_p] == '+')              cl_p++;
    cl_skip();
    int v = 0;
    while (cl_str[cl_p] >= '0' && cl_str[cl_p] <= '9')
        v = v * 10 + (cl_str[cl_p++] - '0');
    return v * sign;
}

static int cl_term(void) {
    int v = cl_prim(); cl_skip();
    while (cl_str[cl_p] == '*' || cl_str[cl_p] == '/') {
        char op = cl_str[cl_p++];
        int  r  = cl_prim();
        v = (op == '*') ? v * r : (r ? v / r : 0);
        cl_skip();
    }
    return v;
}

static int cl_eval(void) {
    int v = cl_term(); cl_skip();
    while (cl_str[cl_p] == '+' || cl_str[cl_p] == '-') {
        char op = cl_str[cl_p++];
        v = (op == '+') ? v + cl_term() : v - cl_term();
        cl_skip();
    }
    return v;
}

static void run_calc(const char *expr) {
    if (!expr || strlen(expr) == 0) {
        kprint_color("Usage: calc <expression>\n", 0x0C);
        kprint_color("  e.g.  calc 3+5*2       calc (10-3)*4      calc 100/4+7\n", 0x07);
        return;
    }
    cl_str = expr; cl_p = 0;
    int result = cl_eval();
    char buf[32];
    kprint_color(expr, 0x0F);
    kprint_color("  =  ", 0x0E);
    int_to_ascii(result, buf);
    kprint_color(buf, 0x0A);
    kprint("\n");
}

static void execute_command(char *input) {
    char cmd[128];
    char arg[128];
    split_command(input, cmd, arg);
    
    if (strcmp(cmd, "help") == 0) {
        kprint_color("Available commands:\n", 0x0E);
        kprint_color("  help              ", 0x0B); kprint("- Display this help menu\n");
        kprint_color("  clear             ", 0x0B); kprint("- Clear screen terminal\n");
        kprint_color("  uptime            ", 0x0B); kprint("- Display uptime ticks\n");
        kprint_color("  sleep             ", 0x0B); kprint("- Sleep for 2 seconds\n");
        kprint_color("  meminfo           ", 0x0B); kprint("- Show RAM memory manager stats\n");
        kprint_color("  memtest           ", 0x0B); kprint("- Run memory stress allocations\n");
        kprint_color("  pwd               ", 0x0B); kprint("- Print current working directory\n");
        kprint_color("  ls [dir]          ", 0x0B); kprint("- List directory contents\n");
        kprint_color("  cd <dir>          ", 0x0B); kprint("- Change current directory\n");
        kprint_color("  mkdir <dir>       ", 0x0B); kprint("- Create folders/directories\n");
        kprint_color("  touch <file>      ", 0x0B); kprint("- Create empty regular files\n");
        kprint_color("  write <file> <txt>", 0x0B); kprint("- Write string text to file\n");
        kprint_color("  cat <file>        ", 0x0B); kprint("- Print file content to terminal\n");
        kprint_color("  cp <src> <dest>   ", 0x0B); kprint("- Copy files in filesystem\n");
        kprint_color("  mv <src> <dest>   ", 0x0B); kprint("- Move/Rename files and folders\n");
        kprint_color("  rm [-rf] <path>   ", 0x0B); kprint("- Remove files/folders (recursive -rf)\n");
        kprint_color("  echo <text>       ", 0x0B); kprint("- Echo text arguments to stdout\n");
        kprint_color("  neofetch          ", 0x0B); kprint("- Print logo and OS details\n");
        kprint_color("  matrix            ", 0x0B); kprint("- Launch hacker matrix rain\n");
        kprint_color("  hack              ", 0x0B); kprint("- Launch system bypass simulation\n");
        kprint_color("  snake            ", 0x0B); kprint("- Play the classic snake game (WASD)\n");
        kprint_color("  calc <expr>      ", 0x0B); kprint("- Math: calc 3+5*2  or  calc (10-3)*4\n");
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (strcmp(cmd, "uptime") == 0) {
        u32 ticks = get_ticks();
        u32 secs = ticks / 100;
        char num_buf[32];
        
        kprint("System Uptime: ");
        int_to_ascii(secs, num_buf);
        kprint_color(num_buf, 0x0E);
        kprint(" seconds (");
        int_to_ascii(ticks, num_buf);
        kprint_color(num_buf, 0x0E);
        kprint(" ticks)\n");
    } else if (strcmp(cmd, "sleep") == 0) {
        kprint("Sleeping for 2 seconds... ");
        sleep(200);
        kprint_color("Awake!\n", 0x0A);
    } else if (strcmp(cmd, "meminfo") == 0) {
        char num_buf[32];
        kprint_color("Heap Status:\n", 0x0E);
        
        kprint("  Total Heap  : ");
        int_to_ascii(get_heap_total(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Used Bytes  : ");
        int_to_ascii(get_heap_used(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Free Usable : ");
        int_to_ascii(get_heap_free(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint(" bytes\n");
        
        kprint("  Block Count : ");
        int_to_ascii(get_heap_block_count(), num_buf);
        kprint_color(num_buf, 0x0A);
        kprint("\n");
    } else if (strcmp(cmd, "memtest") == 0) {
        char ptr_buf[32];
        kprint_color("Running memory tests...\n", 0x0E);
        void *a = kmalloc(16);
        if (a) {
            kprint("Block A (16B): ");
            hex_to_ascii((u32)a, ptr_buf);
            kprint_color(ptr_buf, 0x0B);
            kprint("\n");
            kfree(a);
            kprint("Block A freed.\n");
        } else {
            kprint_color("Allocation failed!\n", 0x0C);
        }
    } else if (strcmp(cmd, "pwd") == 0) {
        kprint(cwd);
        kprint("\n");
    } else if (strcmp(cmd, "echo") == 0) {
        kprint(arg);
        kprint("\n");
    } else if (strcmp(cmd, "touch") == 0) {
        if (strlen(arg) == 0) {
            kprint_color("Usage: touch <filename>\n", 0x0C);
        } else {
            char target[128];
            to_absolute_path(arg, target);
            
            int exists = 0;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    exists = 1;
                    break;
                }
            }
            
            if (exists) {
                kprint_color("touch: file already exists\n", 0x0E);
            } else if (!parent_exists(target)) {
                kprint_color("touch: parent directory not found\n", 0x0C);
            } else {
                if (fs_count < MAX_FILES) {
                    strcpy(fs[fs_count].name, target);
                    fs[fs_count].type = FILE_TYPE_REG;
                    fs[fs_count].content[0] = '\0';
                    fs[fs_count].size = 0;
                    fs[fs_count].active = 1;
                    fs_count++;
                    kprint_color("Created empty file.\n", 0x0A);
                } else {
                    kprint_color("touch: filesystem full\n", 0x0C);
                }
            }
        }
    } else if (strcmp(cmd, "write") == 0) {
        char filename[64], text[256];
        split_two_args(arg, filename, text);
        
        if (strlen(filename) == 0 || strlen(text) == 0) {
            kprint_color("Usage: write <filename> <text>\n", 0x0C);
        } else {
            char target[128];
            to_absolute_path(filename, target);
            
            int idx = -1;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    idx = i;
                    break;
                }
            }
            
            if (idx != -1 && fs[idx].type == FILE_TYPE_DIR) {
                kprint_color("write: cannot write to a directory\n", 0x0C);
            } else if (!parent_exists(target)) {
                kprint_color("write: parent directory not found\n", 0x0C);
            } else {
                if (idx == -1) {
                    if (fs_count < MAX_FILES) {
                        strcpy(fs[fs_count].name, target);
                        fs[fs_count].type = FILE_TYPE_REG;
                        strcpy(fs[fs_count].content, text);
                        fs[fs_count].size = strlen(text);
                        fs[fs_count].active = 1;
                        fs_count++;
                        kprint_color("Created file and wrote contents.\n", 0x0A);
                    } else {
                        kprint_color("write: filesystem full\n", 0x0C);
                    }
                } else {
                    strcpy(fs[idx].content, text);
                    fs[idx].size = strlen(text);
                    kprint_color("File updated.\n", 0x0A);
                }
            }
        }
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (strlen(arg) == 0) {
            kprint_color("Usage: mkdir <dirname>\n", 0x0C);
        } else {
            char target[128];
            to_absolute_path(arg, target);
            
            int exists = 0;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    exists = 1;
                    break;
                }
            }
            
            if (exists) {
                kprint_color("mkdir: path already exists\n", 0x0C);
            } else if (!parent_exists(target)) {
                kprint_color("mkdir: parent directory not found\n", 0x0C);
            } else {
                if (fs_count < MAX_FILES) {
                    strcpy(fs[fs_count].name, target);
                    fs[fs_count].type = FILE_TYPE_DIR;
                    fs[fs_count].size = 0;
                    fs[fs_count].active = 1;
                    fs_count++;
                    kprint_color("Directory created.\n", 0x0A);
                } else {
                    kprint_color("mkdir: filesystem full\n", 0x0C);
                }
            }
        }
    } else if (strcmp(cmd, "ls") == 0) {
        char target[128];
        if (strlen(arg) == 0) {
            strcpy(target, cwd);
        } else {
            to_absolute_path(arg, target);
        }
        
        int exists_dir = 0;
        if (strcmp(target, "/") == 0) {
            exists_dir = 1;
        } else {
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    if (fs[i].type == FILE_TYPE_DIR) {
                        exists_dir = 1;
                    }
                    break;
                }
            }
        }
        
        if (!exists_dir) {
            kprint_color("ls: directory not found: '", 0x0C);
            kprint_color(target, 0x0C);
            kprint_color("'\n", 0x0C);
        } else {
            kprint_color("Directory listing for ", 0x0E);
            kprint_color(target, 0x0E);
            kprint_color(":\n", 0x0E);
            
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && is_in_directory(fs[i].name, target)) {
                    if (fs[i].type == FILE_TYPE_DIR) {
                        kprint_color("  drwxr-xr-x   1 root     root          0 bytes  ", 0x02);
                        char *name_ptr = fs[i].name;
                        int last_slash = 0;
                        for (int s = 0; name_ptr[s] != '\0'; s++) {
                            if (name_ptr[s] == '/') last_slash = s;
                        }
                        kprint_color(name_ptr + last_slash + 1, 0x0B); // Cyan
                    } else {
                        kprint_color("  -rwxr-xr-x   1 root     root       ", 0x02);
                        char sz_str[16];
                        int_to_ascii(fs[i].size, sz_str);
                        int spaces = 5 - strlen(sz_str);
                        for (int s = 0; s < spaces; s++) kprint(" ");
                        kprint_color(sz_str, 0x0A); // Green size
                        kprint_color(" bytes  ", 0x02);
                        
                        char *name_ptr = fs[i].name;
                        int last_slash = 0;
                        for (int s = 0; name_ptr[s] != '\0'; s++) {
                            if (name_ptr[s] == '/') last_slash = s;
                        }
                        kprint_color(name_ptr + last_slash + 1, 0x0F); // White
                    }
                    kprint("\n");
                }
            }
        }
    } else if (strcmp(cmd, "cd") == 0) {
        if (strlen(arg) == 0) {
            strcpy(cwd, "/");
        } else {
            char target[128];
            to_absolute_path(arg, target);
            
            int is_dir = 0;
            if (strcmp(target, "/") == 0) {
                is_dir = 1;
            } else {
                for (int i = 0; i < fs_count; i++) {
                    if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                        if (fs[i].type == FILE_TYPE_DIR) {
                            is_dir = 1;
                        }
                        break;
                    }
                }
            }
            
            if (is_dir) {
                strcpy(cwd, target);
            } else {
                kprint_color("cd: directory not found: '", 0x0C);
                kprint_color(target, 0x0C);
                kprint_color("'\n", 0x0C);
            }
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (strlen(arg) == 0) {
            kprint_color("Usage: cat <filename>\n", 0x0C);
        } else {
            char target[128];
            to_absolute_path(arg, target);
            
            int idx = -1;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    idx = i;
                    break;
                }
            }
            
            if (idx == -1) {
                kprint_color("cat: file not found: '", 0x0C);
                kprint_color(target, 0x0C);
                kprint_color("'\n", 0x0C);
            } else if (fs[idx].type == FILE_TYPE_DIR) {
                kprint_color("cat: cannot display directory\n", 0x0C);
            } else {
                kprint_color(fs[idx].content, 0x0F);
                kprint("\n");
            }
        }
    } else if (strcmp(cmd, "cp") == 0) {
        char src[64], dest[64];
        split_two_args(arg, src, dest);
        
        if (strlen(src) == 0 || strlen(dest) == 0) {
            kprint_color("Usage: cp <source> <dest>\n", 0x0C);
        } else {
            char src_abs[128], dest_abs[128];
            to_absolute_path(src, src_abs);
            to_absolute_path(dest, dest_abs);
            
            int src_idx = -1;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, src_abs) == 0) {
                    src_idx = i;
                    break;
                }
            }
            
            if (src_idx == -1) {
                kprint_color("cp: source path not found: '", 0x0C);
                kprint_color(src_abs, 0x0C);
                kprint_color("'\n", 0x0C);
            } else if (fs[src_idx].type == FILE_TYPE_DIR) {
                kprint_color("cp: copying directories not supported\n", 0x0C);
            } else {
                int dest_is_dir = 0;
                for (int i = 0; i < fs_count; i++) {
                    if (fs[i].active && strcmp(fs[i].name, dest_abs) == 0 && fs[i].type == FILE_TYPE_DIR) {
                        dest_is_dir = 1;
                        break;
                    }
                }
                
                char final_dest[128];
                if (dest_is_dir) {
                    strcpy(final_dest, dest_abs);
                    if (strcmp(dest_abs, "/") != 0) {
                        int flen = strlen(final_dest);
                        final_dest[flen] = '/';
                        final_dest[flen+1] = '\0';
                    }
                    char *src_name = src_abs;
                    int last_slash = 0;
                    for (int s = 0; src_name[s] != '\0'; s++) {
                        if (src_name[s] == '/') last_slash = s;
                    }
                    strcpy(final_dest + strlen(final_dest), src_name + last_slash + 1);
                } else {
                    strcpy(final_dest, dest_abs);
                }
                
                int dest_idx = -1;
                for (int i = 0; i < fs_count; i++) {
                    if (fs[i].active && strcmp(fs[i].name, final_dest) == 0) {
                        dest_idx = i;
                        break;
                    }
                }
                
                if (dest_idx != -1 && fs[dest_idx].type == FILE_TYPE_DIR) {
                    kprint_color("cp: cannot overwrite directory\n", 0x0C);
                } else if (!parent_exists(final_dest)) {
                    kprint_color("cp: target path parent directory not found\n", 0x0C);
                } else {
                    if (dest_idx != -1) {
                        strcpy(fs[dest_idx].content, fs[src_idx].content);
                        fs[dest_idx].size = fs[src_idx].size;
                        kprint_color("File copied (overwritten).\n", 0x0A);
                    } else {
                        if (fs_count < MAX_FILES) {
                            strcpy(fs[fs_count].name, final_dest);
                            fs[fs_count].type = FILE_TYPE_REG;
                            strcpy(fs[fs_count].content, fs[src_idx].content);
                            fs[fs_count].size = fs[src_idx].size;
                            fs[fs_count].active = 1;
                            fs_count++;
                            kprint_color("File copied.\n", 0x0A);
                        } else {
                            kprint_color("cp: filesystem full\n", 0x0C);
                        }
                    }
                }
            }
        }
    } else if (strcmp(cmd, "mv") == 0) {
        char src[64], dest[64];
        split_two_args(arg, src, dest);
        
        if (strlen(src) == 0 || strlen(dest) == 0) {
            kprint_color("Usage: mv <source> <dest>\n", 0x0C);
        } else {
            char src_abs[128], dest_abs[128];
            to_absolute_path(src, src_abs);
            to_absolute_path(dest, dest_abs);
            
            int src_idx = -1;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, src_abs) == 0) {
                    src_idx = i;
                    break;
                }
            }
            
            if (src_idx == -1) {
                kprint_color("mv: source not found: '", 0x0C);
                kprint_color(src_abs, 0x0C);
                kprint_color("'\n", 0x0C);
            } else {
                int dest_is_dir = 0;
                for (int i = 0; i < fs_count; i++) {
                    if (fs[i].active && strcmp(fs[i].name, dest_abs) == 0 && fs[i].type == FILE_TYPE_DIR) {
                        dest_is_dir = 1;
                        break;
                    }
                }
                
                char final_dest[128];
                if (dest_is_dir) {
                    strcpy(final_dest, dest_abs);
                    if (strcmp(dest_abs, "/") != 0) {
                        int flen = strlen(final_dest);
                        final_dest[flen] = '/';
                        final_dest[flen+1] = '\0';
                    }
                    char *src_name = src_abs;
                    int last_slash = 0;
                    for (int s = 0; src_name[s] != '\0'; s++) {
                        if (src_name[s] == '/') last_slash = s;
                    }
                    strcpy(final_dest + strlen(final_dest), src_name + last_slash + 1);
                } else {
                    strcpy(final_dest, dest_abs);
                }
                
                if (fs[src_idx].type == FILE_TYPE_DIR) {
                    int src_len = strlen(src_abs);
                    for (int i = 0; i < fs_count; i++) {
                        if (fs[i].active && i != src_idx) {
                            int match = 1;
                            for (int j = 0; j < src_len; j++) {
                                if (fs[i].name[j] != src_abs[j]) {
                                    match = 0;
                                    break;
                                }
                            }
                            if (match && fs[i].name[src_len] == '/') {
                                char suffix[128];
                                strcpy(suffix, fs[i].name + src_len);
                                strcpy(fs[i].name, final_dest);
                                strcpy(fs[i].name + strlen(final_dest), suffix);
                            }
                        }
                    }
                }
                strcpy(fs[src_idx].name, final_dest);
                kprint_color("Moved/Renamed successfully.\n", 0x0A);
            }
        }
    } else if (strcmp(cmd, "rm") == 0) {
        char path[128];
        int recursive = 0;
        
        if (strcmp(arg, "-rf") == 0) {
            kprint_color("Usage: rm [-rf] <path>\n", 0x0C);
        } else {
            char opt[16], p[128];
            split_two_args(arg, opt, p);
            
            if (strcmp(opt, "-rf") == 0) {
                recursive = 1;
                strcpy(path, p);
            } else {
                strcpy(path, arg);
            }
            
            char target[128];
            to_absolute_path(path, target);
            
            int idx = -1;
            for (int i = 0; i < fs_count; i++) {
                if (fs[i].active && strcmp(fs[i].name, target) == 0) {
                    idx = i;
                    break;
                }
            }
            
            if (idx == -1) {
                if (!recursive) {
                    kprint_color("rm: path not found: '", 0x0C);
                    kprint_color(target, 0x0C);
                    kprint_color("'\n", 0x0C);
                }
            } else if (fs[idx].type == FILE_TYPE_DIR && !recursive) {
                kprint_color("rm: cannot remove directory (use rm -rf)\n", 0x0C);
            } else {
                if (recursive && fs[idx].type == FILE_TYPE_DIR) {
                    int target_len = strlen(target);
                    for (int i = 0; i < fs_count; i++) {
                        if (fs[i].active && i != idx) {
                            int match = 1;
                            for (int j = 0; j < target_len; j++) {
                                if (fs[i].name[j] != target[j]) {
                                    match = 0;
                                    break;
                                }
                            }
                            if (match && fs[i].name[target_len] == '/') {
                                fs[i].active = 0;
                            }
                        }
                    }
                }
                fs[idx].active = 0;
                kprint_color("Removed.\n", 0x0A);
            }
        }
    } else if (strcmp(cmd, "neofetch") == 0) {
        run_neofetch();
    } else if (strcmp(cmd, "matrix") == 0) {
        run_matrix();
    } else if (strcmp(cmd, "hack") == 0) {
        run_hack();
    } else if (strcmp(cmd, "snake") == 0) {
        run_snake();
    } else if (strcmp(cmd, "calc") == 0) {
        run_calc(arg);
    } else if (strlen(cmd) > 0) {
        kprint_color("Unknown command: '", 0x0C);
        kprint_color(cmd, 0x0C);
        kprint_color("'. Type 'help' for commands.\n", 0x0C);
    }
}

static int fs_initialized = 0;

void run_shell() {
    char cmd_buffer[256];
    int cmd_index = 0;
    
    if (!fs_initialized) {
        init_fs();
        fs_initialized = 1;
    }
    
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
        /* Use 'pause' (rep nop) instead of 'hlt'.
         * 'hlt' suspends the CPU until the next interrupt, which means it only
         * wakes on TIMER or KEYBOARD irqs. If the timer IRQ fires first and the
         * keyboard callback already set last_char, we might clear it before
         * reading. 'pause' keeps the CPU spinning with interrupts enabled so we
         * never miss a keypress set during an IRQ. */
        __asm__ __volatile__("pause");
    }
}
