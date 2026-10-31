BUILD := build
DIST := dist
ISO_ROOT := $(BUILD)/iso
KERNEL := $(BUILD)/kernel.elf
ISO := $(DIST)/aqos.iso

CC := gcc
AS := nasm

CFLAGS := -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -mno-mmx -mno-sse -mno-80387 -O2 -Wall -Wextra
LDFLAGS := -m32 -nostdlib -no-pie -Wl,--build-id=none -Wl,-T,linker.ld

.PHONY: all clean check run

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: src/boot.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.o: src/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(BUILD)/boot.o $(BUILD)/kernel.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(BUILD)/boot.o $(BUILD)/kernel.o -lgcc

$(ISO): $(KERNEL) boot/grub/grub.cfg
	mkdir -p $(ISO_ROOT)/boot/grub $(DIST)
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp boot/grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_ROOT)

check: $(KERNEL)
	grub-file --is-x86-multiboot $(KERNEL)

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 128M -usb -device usb-tablet -netdev user,id=net0 -device e1000,netdev=net0

clean:
	rm -rf $(BUILD) $(DIST)
