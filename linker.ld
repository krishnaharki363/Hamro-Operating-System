/* linker.ld */
ENTRY(kernel_main) /* We will set entry in kernel_entry.asm later, for now doesn't matter much as we output binary */
SECTIONS
{
    . = 0x8000; /* Load kernel at 0x8000 (safely above bootloader at 0x7C00) */

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
