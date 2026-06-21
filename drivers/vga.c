#include "vga.h"
#include "ports.h"

// Private function declarations
#define SCROLL_HISTORY_LINES 256
static char scroll_history[SCROLL_HISTORY_LINES][MAX_COLS * 2];
static char shadow_buffer[MAX_ROWS][MAX_COLS * 2];
static int history_count = 0;
static int scroll_offset = 0;

void vga_redraw_screen();
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_col(int offset);

void kprint_at_color(char *message, int col, int row, char attr) {
    int offset;
    if (col >= 0 && row >= 0) {
        offset = get_offset(col, row);
    } else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }

    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, attr);
        // Compute row/col for next char
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void kprint_color(char *message, char attr) {
    kprint_at_color(message, -1, -1, attr);
}

void kprint_at(char *message, int col, int row) {
    kprint_at_color(message, col, row, DEFAULT_COLOR);
}

void kprint(char *message) {
    kprint_color(message, DEFAULT_COLOR);
}

void clear_screen() {
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            shadow_buffer[i][j*2] = ' ';
            shadow_buffer[i][j*2+1] = DEFAULT_COLOR;
        }
    }
    
    scroll_offset = 0;
    vga_redraw_screen();
    set_cursor_offset(get_offset(0, 0));
}

int print_char(char c, int col, int row, char attr) {
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    if (!attr) attr = DEFAULT_COLOR;

    // Error control: print a red 'E' if the coords aren't right
    if (col >= MAX_COLS || row >= MAX_ROWS) {
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-2] = 'E';
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-1] = 0xf4; // Red background
        return get_offset(col, row);
    }

    int offset;
    if (col >= 0 && row >= 0) offset = get_offset(col, row);
    else offset = get_cursor_offset();

    if (c == '\n') {
        row = get_offset_row(offset);
        offset = get_offset(0, row+1);
    } else if (c == '\b') {
        if (offset > 0) {
            offset -= 2;
            int r = get_offset_row(offset);
            int c_idx = get_offset_col(offset);
            shadow_buffer[r][c_idx*2] = ' ';
            shadow_buffer[r][c_idx*2+1] = attr;
        }
    } else {
        int r = get_offset_row(offset);
        int c_idx = get_offset_col(offset);
        if (r < MAX_ROWS && c_idx < MAX_COLS) {
            shadow_buffer[r][c_idx*2] = c;
            shadow_buffer[r][c_idx*2+1] = attr;
        }
        offset += 2;
    }

    // Scroll if needed (simple scrolling)
    if (offset >= MAX_ROWS * MAX_COLS * 2) {
        // Save the top row to history
        for (int j = 0; j < MAX_COLS * 2; j++) {
            scroll_history[history_count % SCROLL_HISTORY_LINES][j] = shadow_buffer[0][j];
        }
        history_count++;
        
        for (int i = 1; i < MAX_ROWS; i++) {
            // copy row i to row i-1
            for (int j = 0; j < MAX_COLS * 2; j++) {
                shadow_buffer[i-1][j] = shadow_buffer[i][j];
            }
        }
        // blank last line
        for (int j = 0; j < MAX_COLS; j++) {
            shadow_buffer[MAX_ROWS-1][j*2] = ' ';
            shadow_buffer[MAX_ROWS-1][j*2+1] = DEFAULT_COLOR;
        }

        offset -= 2 * MAX_COLS;
        // If we are actively looking at history, maybe we shouldn't shift their view immediately, 
        // but for simplicity we will just let it track.
    }

    set_cursor_offset(offset);

    /* Fast path: if not scrolled, write the shadow_buffer directly to vidmem
     * one cell at a time instead of redrawing all 25 rows. */
    if (scroll_offset == 0) {
        char *vidmem2 = (char*) VIDEO_ADDRESS;
        for (int i = 0; i < MAX_ROWS; i++) {
            for (int j = 0; j < MAX_COLS * 2; j++) {
                vidmem2[i * MAX_COLS * 2 + j] = shadow_buffer[i][j];
            }
        }
    } else {
        vga_redraw_screen();
    }
    return offset;
}

int get_cursor_offset() {
    // Use the VGA ports to get the current cursor position
    // 14 = high byte of cursor's offset
    // 15 = low byte of cursor's offset
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8; // High byte: << 8
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2; // Position * size of character cell
}

void set_cursor_offset(int offset) {
    // Similar to get_cursor_offset, but instead of reading we write data
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

int get_offset(int col, int row) { return 2 * (row * MAX_COLS + col); }
int get_offset_row(int offset) { return offset / (2 * MAX_COLS); }
int get_offset_col(int offset) { return (offset - (get_offset_row(offset)*2*MAX_COLS))/2; }

char get_char_at(int col, int row) {
    if (col >= MAX_COLS || row >= MAX_ROWS) return ' ';
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    return vidmem[get_offset(col, row)];
}

char get_color_at(int col, int row) {
    if (col >= MAX_COLS || row >= MAX_ROWS) return DEFAULT_COLOR;
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    return vidmem[get_offset(col, row) + 1];
}

void print_char_at_color(char c, int col, int row, char attr) {
    print_char(c, col, row, attr);
}

void vga_redraw_screen() {
    char *vidmem = (char*) VIDEO_ADDRESS;
    for (int i = 0; i < MAX_ROWS; i++) {
        int history_idx = history_count - scroll_offset + i;
        if (history_idx < 0) {
            // Not enough history, draw blanks
            for (int j = 0; j < MAX_COLS; j++) {
                vidmem[(i * MAX_COLS + j) * 2] = ' ';
                vidmem[(i * MAX_COLS + j) * 2 + 1] = DEFAULT_COLOR;
            }
        } else if (history_idx < history_count) {
            // Draw from history
            for (int j = 0; j < MAX_COLS * 2; j++) {
                vidmem[i * MAX_COLS * 2 + j] = scroll_history[history_idx % SCROLL_HISTORY_LINES][j];
            }
        } else {
            // Draw from shadow buffer
            int shadow_i = history_idx - history_count;
            for (int j = 0; j < MAX_COLS * 2; j++) {
                vidmem[i * MAX_COLS * 2 + j] = shadow_buffer[shadow_i][j];
            }
        }
    }
}

void vga_scroll_up() {
    if (scroll_offset < SCROLL_HISTORY_LINES && scroll_offset < history_count) {
        scroll_offset++;
        vga_redraw_screen();
    }
}

void vga_scroll_down() {
    if (scroll_offset > 0) {
        scroll_offset--;
        vga_redraw_screen();
    }
}

char* get_shadow_buffer() {
    return (char*)shadow_buffer;
}

/* Write a character directly to the shadow buffer at (col, row) and redraw.
 * Does NOT move the hardware cursor, so it is safe for background tasks. */
void vga_poke(char c, char attr, int col, int row) {
    if (col < 0 || col >= MAX_COLS || row < 0 || row >= MAX_ROWS) return;
    shadow_buffer[row][col * 2]     = c;
    shadow_buffer[row][col * 2 + 1] = attr;
    vga_redraw_screen();
}
