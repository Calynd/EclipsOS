CC         := gcc
AS_CC      := gcc
HOSTCC     := cc
LD         := ld
OBJCOPY    := objcopy

STAGE2_SECTORS     := 8
KERNEL_MAX_SECTORS := 896
IMAGE_SIZE_KIB     := 1440
BOOT_DOTS          := 18
VOLUME_LABEL       := UDOS

BUILD := build
IMAGE := $(BUILD)/udos.img
ROOTFS := $(BUILD)/rootfs

KERNEL_SRCS := kernel/main.c kernel/vga.c kernel/gdt.c kernel/idt.c kernel/exception.c kernel/pic.c \
               kernel/timer.c kernel/keyboard.c kernel/kbd_map.c kernel/charbuf.c
KERNEL_HEADERS := $(wildcard kernel/*.h)
KERNEL_DEFINES ?=
KERNEL_OBJS := $(patsubst kernel/%.c,$(BUILD)/kernel/%.o,$(KERNEL_SRCS))

CFLAGS := -std=c11 -m64 -O2 -Wall -Wextra -Werror -ffreestanding -fno-pic -fno-pie \
          -fno-stack-protector -fno-asynchronous-unwind-tables -fcf-protection=none \
          -fno-tree-loop-distribute-patterns -mno-red-zone -mgeneral-regs-only $(KERNEL_DEFINES)

HOSTCFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Werror -Iinclude

STAGE1_ASFLAGS := -m32 -c -DSTAGE2_SECTORS=$(STAGE2_SECTORS)
STAGE2_ASFLAGS := -m64 -c -DBOOT_DOTS=$(BOOT_DOTS)

MKUDFS := $(BUILD)/tools/mkudfs
UDFSCK := $(BUILD)/tools/udfsck

.PHONY: all clean run check test

all: $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tools/%: tools/%.c include/udfs.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILD)/stage1.o: boot/stage1.S | $(BUILD)
	$(AS_CC) $(STAGE1_ASFLAGS) -o $@ $<

$(BUILD)/stage1.bin: $(BUILD)/stage1.o
	$(LD) -m elf_i386 --oformat binary -Ttext 0x7C00 -o $@ $<
	@test `wc -c < $@` -eq 512 || (echo "stage1 must be exactly 512 bytes" && exit 1)

$(BUILD)/stage2.o: boot/stage2.S | $(BUILD)
	$(AS_CC) $(STAGE2_ASFLAGS) -o $@ $<

$(BUILD)/stage2.bin: $(BUILD)/stage2.o
	$(LD) -m elf_x86_64 --oformat binary -Ttext 0x7E00 -o $@ $<
	@test `wc -c < $@` -le $$(($(STAGE2_SECTORS) * 512)) || (echo "stage2 does not fit in $(STAGE2_SECTORS) sectors" && exit 1)

$(BUILD)/kernel/%.o: kernel/%.c $(KERNEL_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/kernel.elf: $(KERNEL_OBJS) kernel/kernel.ld
	$(LD) -m elf_x86_64 -nostdlib -T kernel/kernel.ld -o $@ $(KERNEL_OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@test `wc -c < $@` -le $$(($(KERNEL_MAX_SECTORS) * 512)) || (echo "kernel does not fit in $(KERNEL_MAX_SECTORS) sectors" && exit 1)

$(BUILD)/udbl.bin: $(BUILD)/stage1.bin $(BUILD)/stage2.bin
	dd if=/dev/zero of=$@ bs=512 count=16 status=none
	dd if=$(BUILD)/stage1.bin of=$@ conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc status=none

$(BUILD)/rootfs.stamp: $(BUILD)/udbl.bin $(BUILD)/kernel.bin
	rm -rf $(ROOTFS)
	mkdir -p $(ROOTFS)/sys/boot $(ROOTFS)/bin $(ROOTFS)/usr/root $(ROOTFS)/conf $(ROOTFS)/tmp $(ROOTFS)/dev
	cp $(BUILD)/kernel.bin $(ROOTFS)/sys/boot/kernel
	cp $(BUILD)/udbl.bin $(ROOTFS)/sys/boot/udbl
	touch $@

$(IMAGE): $(MKUDFS) $(BUILD)/udbl.bin $(BUILD)/rootfs.stamp
	$(MKUDFS) --boot-area $(BUILD)/udbl.bin --root $(ROOTFS) --size-kib $(IMAGE_SIZE_KIB) --label $(VOLUME_LABEL) -o $@

check: $(IMAGE) $(UDFSCK)
	$(UDFSCK) $(IMAGE)
	$(UDFSCK) -x sys/boot/kernel $(IMAGE) | cmp - $(BUILD)/kernel.bin

TESTS := $(BUILD)/tests/test_kbd_map $(BUILD)/tests/test_charbuf $(BUILD)/tests/test_idt

$(BUILD)/tests/test_kbd_map: tests/test_kbd_map.c kernel/kbd_map.c kernel/kbd_map.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel -o $@ tests/test_kbd_map.c kernel/kbd_map.c

$(BUILD)/tests/test_charbuf: tests/test_charbuf.c kernel/charbuf.c kernel/charbuf.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel -o $@ tests/test_charbuf.c kernel/charbuf.c

$(BUILD)/tests/test_idt: tests/test_idt.c kernel/idt.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -Ikernel -o $@ tests/test_idt.c

test: $(TESTS)
	@for t in $(TESTS); do $$t || exit 1; done

clean:
	rm -rf $(BUILD)

run: $(IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE)
