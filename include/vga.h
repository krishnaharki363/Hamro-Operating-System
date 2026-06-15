#ifndef VGA_H
#define VGA_H

#define VIDEO_ADDRESS 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80

// VGA Colors
#define VGA_BLACK 0x0
#define VGA_BLUE 0x1
#define VGA_GREEN 0x2
#define VGA_CYAN 0x3
#define VGA_RED 0x4
#define VGA_MAGENTA 0x5
#define VGA_BROWN 0x6
#define VGA_LIGHT_GRAY 0x7
#define VGA_DARK_GRAY 0x8
#define VGA_LIGHT_BLUE 0x9
#define VGA_LIGHT_GREEN 0xA
#define VGA_LIGHT_CYAN 0xB
#define VGA_LIGHT_RED 0xC
#define VGA_LIGHT_MAGENTA 0xD
#define VGA_YELLOW 0xE
#define VGA_WHITE 0xF

// Macro to create attribute byte
#define VGA_COLOR(fg, bg) ((fg & 0x0F) | ((bg & 0x07) << 4))

// Attribute bytes
#define WHITE_ON_BLACK 0x0f
#define DEFAULT_COLOR 0x0a // Light Green on Black (Hacker Theme)

// Screen device I/O ports
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

void kprint(char *message);
void kprint_at(char *message, int col, int row);
void kprint_color(char *message, char attr);
void kprint_at_color(char *message, int col, int row, char attr);
void clear_screen();

#endif
