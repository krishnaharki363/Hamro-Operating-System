#include "keyboard.h"
#include "ports.h"
#include "vga.h"

/* ---------------------------------------------------------------------------
 * Ring buffer for keypresses.
 * Using a 16-slot circular buffer ensures that rapid keypresses are never
 * silently dropped because the shell loop was momentarily occupied by a timer
 * IRQ or any other work.  The old single-variable design meant that if a
 * second key arrived before the shell called clear_last_char(), the first
 * character was overwritten and lost forever.
 * -------------------------------------------------------------------------*/
#define KB_BUF_SIZE 16
static volatile char kb_buf[KB_BUF_SIZE];
static volatile int  kb_head = 0;   /* next slot to write (producer) */
static volatile int  kb_tail = 0;   /* next slot to read  (consumer) */

static int shift_pressed = 0;

const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
};

const char scancode_to_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\b',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
};

static u32 keyboard_callback(registers_t *regs) {
    (void)regs;
    /* The PIC leaves us the scancode in port 0x60 */
    u8 scancode = port_byte_in(0x60);

    /* Shift press / release */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return (u32)regs; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return (u32)regs; }

    /* Ignore all other break codes (key-release events) */
    if (scancode & 0x80) return (u32)regs;

    /* Map make code to ASCII */
    if (scancode < sizeof(scancode_to_ascii)) {
        char ascii = shift_pressed
                   ? scancode_to_ascii_shift[scancode]
                   : scancode_to_ascii[scancode];
        if (ascii != 0) {
            /* Enqueue into ring buffer (drop silently if full) */
            int next_head = (kb_head + 1) % KB_BUF_SIZE;
            if (next_head != kb_tail) {   /* buffer not full */
                kb_buf[kb_head] = ascii;
                kb_head = next_head;
            }
        }
    }
    return (u32)regs;
}

void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}

/* Returns the oldest character in the buffer, or 0 if empty.
 * Does NOT remove it from the buffer — call clear_last_char() to consume. */
char get_last_char() {
    if (kb_tail == kb_head) return 0;   /* buffer empty */
    return kb_buf[kb_tail];
}

/* Consume (remove) the oldest character from the buffer. */
void clear_last_char() {
    if (kb_tail != kb_head) {
        kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    }
}
