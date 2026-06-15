[bits 32]
[extern kernel_main] ; Define calling point. Must have same name as kernel.c 'kernel_main' function

call kernel_main ; Calls the C function. The linker will know where it is placed in memory
jmp $            ; Hang forever when we return from the kernel
