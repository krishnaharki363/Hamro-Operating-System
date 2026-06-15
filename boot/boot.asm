[org 0x7c00]    ; BIOS loads the bootloader at memory address 0x7c00

KERNEL_OFFSET equ 0x1000 ; The memory offset to which we will load our kernel

    ; Explicitly initialize segment registers to 0
    cli                 ; Clear interrupts during stack setup
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00      ; Set up stack pointer safely below bootloader
    sti                 ; Restore interrupts

    mov [BOOT_DRIVE], dl ; Save the boot drive number! BIOS stores it in DL.

    ; 1. Print a message in 16-bit real mode using BIOS interrupts
    mov bx, MSG_REAL_MODE
    call print_string

    ; 2. Load the kernel from disk
    call load_kernel

    ; 3. Switch to 32-bit protected mode
    call switch_to_pm

    jmp $ ; Hang (should never be reached if switch_to_pm works)

%include "boot/print_string.asm"
%include "boot/disk_load.asm"
%include "boot/gdt.asm"
%include "boot/print_string_pm.asm"
%include "boot/switch_to_pm.asm"

[bits 32]
; This is where we arrive after switching to and initializing 32-bit protected mode.
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm ; Use our 32-bit print routine

    ; Jump to the address of our loaded kernel code
    call KERNEL_OFFSET 

    jmp $ ; Hang

; Global variables
BOOT_DRIVE      db 0
MSG_REAL_MODE   db "Started in 16-bit Real Mode", 0
MSG_PROT_MODE   db "Successfully landed in 32-bit Protected Mode", 0
MSG_LOAD_KERNEL db "Loading kernel into memory.", 0

; Bootsector padding
times 510-($-$$) db 0
dw 0xaa55
