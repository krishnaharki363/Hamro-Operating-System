# Makefile for Educational OS

C_SOURCES = $(wildcard kernel/*.c drivers/*.c memory/*.c process/*.c filesystem/*.c shell/*.c)
HEADERS = $(wildcard include/*.h)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o}

CC = clang
LD = /opt/homebrew/bin/x86_64-elf-ld
CFLAGS = -g -target i386-elf -ffreestanding -Wall -Wextra -fno-exceptions -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Default target
all: os-image

# Run qemu
run: all
	qemu-system-i386 -fda os-image

# This is the actual disk image that the computer loads
# It is the combination of our compiled bootsector and kernel
os-image: boot/boot.bin kernel.bin
	cat $^ > os-image
	# Pad to standard 1.44MB floppy size (2880 sectors * 512 bytes = 1474560 bytes)
	dd if=/dev/zero bs=512 count=2880 >> os-image 2>/dev/null
	truncate -s 1474560 os-image

# This builds the binary of our kernel from object files:
#  - boot/kernel_entry.o: entry point
#  - kernel/interrupt.o: assembly ISR wrappers
#  - ${OBJ}: compiled C objects
kernel.bin: boot/kernel_entry.o kernel/interrupt.o ${OBJ}
	${LD} ${LDFLAGS} -o $@ $^ --oformat binary

# Generic rules for wildcards
# To make an object, always compile from its .c
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

# Clean up
clean:
	rm -f *.bin *.o os-image
	rm -f kernel/*.o boot/*.bin drivers/*.o boot/*.o memory/*.o process/*.o filesystem/*.o shell/*.o

