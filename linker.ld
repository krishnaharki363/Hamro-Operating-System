/* linker.ld */
ENTRY(kernel_main) /* We will set entry in kernel_entry.asm later, for now doesn't matter much as we output binary */
SECTIONS
{
    . = 0x1000; /* Load kernel at 0x1000 */

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
