; load DH sectors to ES:BX from drive DL
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string
    call print_nl

    ; Save drive number
    mov dl, [BOOT_DRIVE]

    ; We want to load 50 sectors (more than enough for the kernel)
    mov di, 50              ; Loop counter (sectors to read)
    mov bx, KERNEL_OFFSET   ; Load destination in memory (0x1000)

    ; Start position on floppy
    mov ch, 0               ; Cylinder 0
    mov dh, 0               ; Head 0
    mov cl, 2               ; Sector 2 (Sector 1 is boot sector)

.read_sector_loop:
    push dx
    push cx

    mov ah, 0x02            ; BIOS read sector
    mov al, 1               ; Read exactly 1 sector
    int 0x13
    jc disk_error

    pop cx
    pop dx

    ; Move memory pointer forward by 512 bytes (1 sector size)
    add bx, 512

    ; Increment sector number
    inc cl
    cmp cl, 19              ; 18 sectors per track on standard 1.44MB floppy
    jl .continue

    ; If sector > 18, reset to sector 1 and go to next head
    mov cl, 1
    inc dh                  ; Next head
    cmp dh, 2               ; 2 heads per cylinder (0 and 1)
    jl .continue

    ; If head >= 2, reset head to 0 and go to next cylinder
    mov dh, 0
    inc ch                  ; Next cylinder

.continue:
    dec di                  ; Decrement loop counter
    jnz .read_sector_loop

    ret

disk_error:
    mov bx, DISK_ERR_MSG
    call print_string
    jmp $

DISK_ERR_MSG db "Disk read error!", 0
