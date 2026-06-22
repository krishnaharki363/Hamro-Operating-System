; disk_load.asm — loads the kernel from floppy into memory at physical 0x8000
;
; Floppy DMA cannot cross a 64KB physical boundary (0x10000, 0x20000, etc) during a single read.
; We load 71 sectors (36 KB). 0x8000 + 36KB = 0x11000, which crosses the 0x10000 boundary.
; To fix this, we split the read precisely at the 0x10000 boundary.
;
; Memory layout:
; Read 1: Cyl 0, Hd 0, Sec 2-18 (17 sec)  ES=0x0800 BX=0x0000 (0x08000 -> 0x0A200)
; Read 2: Cyl 0, Hd 1, Sec 1-18 (18 sec)  ES=0x0A20 BX=0x0000 (0x0A200 -> 0x0C600)
; Read 3: Cyl 1, Hd 0, Sec 1-18 (18 sec)  ES=0x0C60 BX=0x0000 (0x0C600 -> 0x0EA00)
; Read 4: Cyl 1, Hd 1, Sec 1-11 (11 sec)  ES=0x0EA0 BX=0x0000 (0x0EA00 -> 0x10000) <- boundary
; Read 5: Cyl 1, Hd 1, Sec 12-18( 7 sec)  ES=0x1000 BX=0x0000 (0x10000 -> 0x10E00)
;
; Total = 17+18+18+11+7 = 71 sectors.

load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string
    call print_nl

    ; ---- Read 1: 17 sectors ----
    mov ax, 0x0800
    mov es, ax
    mov bx, 0x0000
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02
    mov al, 17
    mov ch, 0
    mov dh, 0
    mov cl, 2
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '1'
    int 0x10

    ; ---- Read 2: 18 sectors ----
    mov ax, 0x0A20
    mov es, ax
    mov ah, 0x02
    mov al, 18
    mov ch, 0
    mov dh, 1
    mov cl, 1
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '2'
    int 0x10

    ; ---- Read 3: 18 sectors ----
    mov ax, 0x0C60
    mov es, ax
    mov ah, 0x02
    mov al, 18
    mov ch, 1
    mov dh, 0
    mov cl, 1
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '3'
    int 0x10

    ; ---- Read 4: 11 sectors (stops at 0x10000 boundary) ----
    mov ax, 0x0EA0
    mov es, ax
    mov ah, 0x02
    mov al, 11
    mov ch, 1
    mov dh, 1
    mov cl, 1
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '4'
    int 0x10

    ; ---- Read 5: 7 sectors (starts at 0x10000) ----
    mov ax, 0x1000
    mov es, ax
    mov ah, 0x02
    mov al, 7
    mov ch, 1
    mov dh, 1
    mov cl, 12   ; sector 12
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '5'
    int 0x10

    ; ---- Read 6: 18 sectors (starts at 0x10E00) ----
    mov ax, 0x10E0
    mov es, ax
    mov ah, 0x02
    mov al, 18
    mov ch, 2    ; Cylinder 2
    mov dh, 0    ; Head 0
    mov cl, 1    ; sector 1
    int 0x13
    jc disk_error
    mov ah, 0x0e
    mov al, '6'
    int 0x10

    ; Restore ES=0 for the rest of the bootloader
    mov ax, 0
    mov es, ax

    call print_nl
    ret

disk_error:
    mov ax, 0
    mov es, ax
    mov bx, DISK_ERR_MSG
    call print_string
    jmp $

DISK_ERR_MSG db "Disk read error!", 0
