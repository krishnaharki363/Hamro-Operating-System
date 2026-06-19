; Load DH sectors to ES:BX from drive DL
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print_string
    call print_nl

    mov bx, KERNEL_OFFSET   ; Load destination in memory (0x1000)

    ; 1. Read Track 0 (Cylinder 0, Head 0): sectors 2..18 (17 sectors)
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02            ; BIOS read sectors
    mov al, 17              ; read 17 sectors
    mov ch, 0               ; cylinder 0
    mov dh, 0               ; head 0
    mov cl, 2               ; start sector 2
    int 0x13
    jc disk_error

    ; Print progress '1'
    mov ah, 0x0e
    mov al, '1'
    int 0x10

    add bx, 17 * 512        ; advance memory buffer pointer by 17 sectors

    ; 2. Read Track 1 (Cylinder 0, Head 1): sectors 1..18 (18 sectors)
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02
    mov al, 18              ; read 18 sectors
    mov ch, 0               ; cylinder 0
    mov dh, 1               ; head 1
    mov cl, 1               ; start sector 1
    int 0x13
    jc disk_error

    ; Print progress '2'
    mov ah, 0x0e
    mov al, '2'
    int 0x10

    add bx, 18 * 512        ; advance memory buffer pointer by 18 sectors

    ; 3. Read Track 2 (Cylinder 1, Head 0): sectors 1..18 (18 sectors)
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02
    mov al, 18              ; read 18 sectors
    mov ch, 1               ; cylinder 1
    mov dh, 0               ; head 0
    mov cl, 1               ; start sector 1
    int 0x13
    jc disk_error

    ; Print progress '3'
    mov ah, 0x0e
    mov al, '3'
    int 0x10

    add bx, 18 * 512

    ; 4. Read Track 3 (Cylinder 1, Head 1): sectors 1..18 (18 sectors)
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02
    mov al, 18              ; read 18 sectors
    mov ch, 1               ; cylinder 1
    mov dh, 1               ; head 1
    mov cl, 1               ; start sector 1
    int 0x13
    jc disk_error

    ; Print progress '4'
    mov ah, 0x0e
    mov al, '4'
    int 0x10

    add bx, 18 * 512

    ; 5. Read Track 4 (Cylinder 2, Head 0): sectors 1..18 (18 sectors)
    mov dl, [BOOT_DRIVE]
    mov ah, 0x02
    mov al, 18              ; read 18 sectors
    mov ch, 2               ; cylinder 2
    mov dh, 0               ; head 0
    mov cl, 1               ; start sector 1
    int 0x13
    jc disk_error

    ; Print progress '5'
    mov ah, 0x0e
    mov al, '5'
    int 0x10

    call print_nl
    ret

disk_error:
    mov bx, DISK_ERR_MSG
    call print_string
    jmp $

DISK_ERR_MSG db "Disk read error!", 0
