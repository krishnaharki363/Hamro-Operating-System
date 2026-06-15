#include "vga.h"
#include "ports.h"

// Private function declarations
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
    int screen_size = MAX_COLS * MAX_ROWS;
    char *screen = (char*) VIDEO_ADDRESS;

    for (int i = 0; i < screen_size; i++) {
        screen[i*2] = ' ';
        screen[i*2+1] = DEFAULT_COLOR;
    }
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
            vidmem[offset] = ' ';
            vidmem[offset+1] = attr;
        }
    } else {
        vidmem[offset] = c;
        vidmem[offset+1] = attr;
        offset += 2;
    }

    // Scroll if needed (simple scrolling)
    if (offset >= MAX_ROWS * MAX_COLS * 2) {
        for (int i = 1; i < MAX_ROWS; i++) {
            // copy row i to row i-1
            for (int j = 0; j < MAX_COLS * 2; j++) {
                vidmem[get_offset(0, i-1) + j] = vidmem[get_offset(0, i) + j];
            }
        }
        // blank last line
        char *last_line = (char*) (get_offset(0, MAX_ROWS-1) + VIDEO_ADDRESS);
        for (int i = 0; i < MAX_COLS; i++) {
            last_line[i*2] = ' ';
            last_line[i*2+1] = DEFAULT_COLOR;
        }

        offset -= 2 * MAX_COLS;
    }

    set_cursor_offset(offset);
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
