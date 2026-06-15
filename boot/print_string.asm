; 16-bit Real Mode String Printing Routine
; Prints a null-terminated string pointed to by BX

print_string:
    pusha           ; Save registers
    mov ah, 0x0e    ; BIOS scrolling teletype function

.loop:
    mov al, [bx]    ; Get character
    cmp al, 0       ; Check for null terminator
    je .done        ; If null, we're done
    int 0x10        ; Print character using BIOS interrupt
    inc bx          ; Move to next character
    jmp .loop       ; Loop

.done:
    popa            ; Restore registers
    ret

print_nl:
    pusha
    mov ah, 0x0e
    mov al, 0x0a    ; newline
    int 0x10
    mov al, 0x0d    ; carriage return
    int 0x10
    popa
    ret
