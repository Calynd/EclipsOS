KERNEL_CC = gcc
BOOT_CC = gcc
HOST_CC = cc
LD = ld
OBJCOPY = objcopy

STAGE2_SECTORS = 8
KERNEL_MAX_SECTORS = 896
IMAGE_SIZE_KIB = 1440
BOOT_DOTS = 18
VOLUME_LABEL = UDOS
KERNEL_DEFINES ?=

BUILD = build
IMAGE = ${BUILD}/udos.img
ROOTFS = ${BUILD}/rootfs
MKUDFS = ${BUILD}/tools/mkudfs
UDFSCK = ${BUILD}/tools/udfsck

KERNEL_HEADERS = \
    kernel/boot_tables.h \
    kernel/charbuf.h \
    kernel/exception.h \
    kernel/gdt.h \
    kernel/heap.h \
    kernel/idt.h \
    kernel/interrupt.h \
    kernel/io.h \
    kernel/kbd_map.h \
    kernel/keyboard.h \
    kernel/layout.h \
    kernel/memory.h \
    kernel/pic.h \
    kernel/pmm.h \
    kernel/timer.h \
    kernel/udbl.h \
    kernel/vga.h \
    kernel/vm.h \
    kernel/vmm.h

KERNEL_OBJS = \
    ${BUILD}/kernel/entry.o \
    ${BUILD}/kernel/main.o \
    ${BUILD}/kernel/vga.o \
    ${BUILD}/kernel/gdt.o \
    ${BUILD}/kernel/idt.o \
    ${BUILD}/kernel/exception.o \
    ${BUILD}/kernel/pic.o \
    ${BUILD}/kernel/timer.o \
    ${BUILD}/kernel/keyboard.o \
    ${BUILD}/kernel/kbd_map.o \
    ${BUILD}/kernel/charbuf.o \
    ${BUILD}/kernel/pmm.o \
    ${BUILD}/kernel/heap.o \
    ${BUILD}/kernel/memory.o \
    ${BUILD}/kernel/vmm.o \
    ${BUILD}/kernel/vm.o

KERNEL_CFLAGS = -std=c11 -m64 -O2 -Wall -Wextra -Werror -ffreestanding -fno-pic -fno-pie \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fcf-protection=none \
    -fno-tree-loop-distribute-patterns -mno-red-zone -mgeneral-regs-only ${KERNEL_DEFINES}

HOST_CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Werror -Iinclude

STAGE1_ASFLAGS = -m32 -c -DSTAGE2_SECTORS=${STAGE2_SECTORS}
STAGE2_ASFLAGS = -m64 -c -DBOOT_DOTS=${BOOT_DOTS}

TESTS = \
    ${BUILD}/tests/test_kbd_map \
    ${BUILD}/tests/test_charbuf \
    ${BUILD}/tests/test_idt \
    ${BUILD}/tests/test_pmm \
    ${BUILD}/tests/test_heap \
    ${BUILD}/tests/test_vmm \
    ${BUILD}/tests/test_boot_tables

.PHONY: all clean run check test

all: ${IMAGE}

${BUILD}/tools/mkudfs: tools/mkudfs.c include/udfs.h
	mkdir -p ${BUILD}/tools
	${HOST_CC} ${HOST_CFLAGS} -o ${BUILD}/tools/mkudfs tools/mkudfs.c

${BUILD}/tools/udfsck: tools/udfsck.c include/udfs.h
	mkdir -p ${BUILD}/tools
	${HOST_CC} ${HOST_CFLAGS} -o ${BUILD}/tools/udfsck tools/udfsck.c

${BUILD}/stage1.o: boot/stage1.S
	mkdir -p ${BUILD}
	${BOOT_CC} ${STAGE1_ASFLAGS} -o ${BUILD}/stage1.o boot/stage1.S

${BUILD}/stage1.bin: ${BUILD}/stage1.o
	${LD} -m elf_i386 --oformat binary -Ttext 0x7C00 -o ${BUILD}/stage1.bin ${BUILD}/stage1.o
	@test $$(( $$(wc -c < ${BUILD}/stage1.bin) )) -eq 512 || (echo "stage1 must be exactly 512 bytes" && exit 1)

${BUILD}/stage2.o: boot/stage2.S
	mkdir -p ${BUILD}
	${BOOT_CC} ${STAGE2_ASFLAGS} -o ${BUILD}/stage2.o boot/stage2.S

${BUILD}/stage2.bin: ${BUILD}/stage2.o
	${LD} -m elf_x86_64 --oformat binary -Ttext 0x7E00 -o ${BUILD}/stage2.bin ${BUILD}/stage2.o
	@test $$(( $$(wc -c < ${BUILD}/stage2.bin) )) -le $$(( ${STAGE2_SECTORS} * 512 )) || (echo "stage2 does not fit in ${STAGE2_SECTORS} sectors" && exit 1)

${BUILD}/kernel/entry.o: kernel/entry.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -c -o ${BUILD}/kernel/entry.o kernel/entry.c

${BUILD}/kernel/main.o: kernel/main.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/main.o kernel/main.c

${BUILD}/kernel/vga.o: kernel/vga.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/vga.o kernel/vga.c

${BUILD}/kernel/gdt.o: kernel/gdt.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/gdt.o kernel/gdt.c

${BUILD}/kernel/idt.o: kernel/idt.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/idt.o kernel/idt.c

${BUILD}/kernel/exception.o: kernel/exception.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/exception.o kernel/exception.c

${BUILD}/kernel/pic.o: kernel/pic.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/pic.o kernel/pic.c

${BUILD}/kernel/timer.o: kernel/timer.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/timer.o kernel/timer.c

${BUILD}/kernel/keyboard.o: kernel/keyboard.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/keyboard.o kernel/keyboard.c

${BUILD}/kernel/kbd_map.o: kernel/kbd_map.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/kbd_map.o kernel/kbd_map.c

${BUILD}/kernel/charbuf.o: kernel/charbuf.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/charbuf.o kernel/charbuf.c

${BUILD}/kernel/pmm.o: kernel/pmm.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/pmm.o kernel/pmm.c

${BUILD}/kernel/heap.o: kernel/heap.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/heap.o kernel/heap.c

${BUILD}/kernel/memory.o: kernel/memory.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/memory.o kernel/memory.c

${BUILD}/kernel/vmm.o: kernel/vmm.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/vmm.o kernel/vmm.c

${BUILD}/kernel/vm.o: kernel/vm.c ${KERNEL_HEADERS}
	mkdir -p ${BUILD}/kernel
	${KERNEL_CC} ${KERNEL_CFLAGS} -mcmodel=kernel -c -o ${BUILD}/kernel/vm.o kernel/vm.c

${BUILD}/kernel.elf: ${KERNEL_OBJS} kernel/kernel.ld
	${LD} -m elf_x86_64 -nostdlib -T kernel/kernel.ld -o ${BUILD}/kernel.elf ${KERNEL_OBJS}
	@test "$$(nm ${BUILD}/kernel.elf | awk '/ udk_entry$$/ {print $$1}')" = "0000000000100000" || (echo "udk_entry must be at 0x100000" && exit 1)
	@nm ${BUILD}/kernel.elf | awk '/ udk_main$$/ {print $$1}' | grep -q '^ffffffff80' || (echo "udk_main must be in the higher half" && exit 1)

${BUILD}/kernel.bin: ${BUILD}/kernel.elf
	${OBJCOPY} -O binary ${BUILD}/kernel.elf ${BUILD}/kernel.bin
	@test $$(( $$(wc -c < ${BUILD}/kernel.bin) )) -le $$(( ${KERNEL_MAX_SECTORS} * 512 )) || (echo "kernel does not fit in ${KERNEL_MAX_SECTORS} sectors" && exit 1)

${BUILD}/udbl.bin: ${BUILD}/stage1.bin ${BUILD}/stage2.bin
	dd if=/dev/zero of=${BUILD}/udbl.bin bs=512 count=16 2>/dev/null
	dd if=${BUILD}/stage1.bin of=${BUILD}/udbl.bin conv=notrunc 2>/dev/null
	dd if=${BUILD}/stage2.bin of=${BUILD}/udbl.bin bs=512 seek=1 conv=notrunc 2>/dev/null

${BUILD}/rootfs.stamp: ${BUILD}/udbl.bin ${BUILD}/kernel.bin
	rm -rf ${ROOTFS}
	mkdir -p ${ROOTFS}/sys/boot ${ROOTFS}/bin ${ROOTFS}/usr/root ${ROOTFS}/conf ${ROOTFS}/tmp ${ROOTFS}/dev
	cp ${BUILD}/kernel.bin ${ROOTFS}/sys/boot/kernel
	cp ${BUILD}/udbl.bin ${ROOTFS}/sys/boot/udbl
	touch ${BUILD}/rootfs.stamp

${IMAGE}: ${MKUDFS} ${BUILD}/udbl.bin ${BUILD}/rootfs.stamp
	${MKUDFS} --boot-area ${BUILD}/udbl.bin --root ${ROOTFS} --size-kib ${IMAGE_SIZE_KIB} --label ${VOLUME_LABEL} -o ${IMAGE}

check: ${IMAGE} ${UDFSCK}
	${UDFSCK} ${IMAGE}
	${UDFSCK} -x sys/boot/kernel ${IMAGE} | cmp - ${BUILD}/kernel.bin

${BUILD}/tests/test_kbd_map: tests/test_kbd_map.c kernel/kbd_map.c kernel/kbd_map.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_kbd_map tests/test_kbd_map.c kernel/kbd_map.c

${BUILD}/tests/test_charbuf: tests/test_charbuf.c kernel/charbuf.c kernel/charbuf.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_charbuf tests/test_charbuf.c kernel/charbuf.c

${BUILD}/tests/test_idt: tests/test_idt.c kernel/idt.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_idt tests/test_idt.c

${BUILD}/tests/test_pmm: tests/test_pmm.c kernel/pmm.c kernel/pmm.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_pmm tests/test_pmm.c kernel/pmm.c

${BUILD}/tests/test_heap: tests/test_heap.c kernel/heap.c kernel/heap.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_heap tests/test_heap.c kernel/heap.c

${BUILD}/tests/test_vmm: tests/test_vmm.c kernel/vmm.c kernel/vmm.h kernel/pmm.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_vmm tests/test_vmm.c kernel/vmm.c

${BUILD}/tests/test_boot_tables: tests/test_boot_tables.c kernel/vmm.c kernel/vmm.h kernel/boot_tables.h kernel/layout.h
	mkdir -p ${BUILD}/tests
	${HOST_CC} ${HOST_CFLAGS} -Ikernel -o ${BUILD}/tests/test_boot_tables tests/test_boot_tables.c kernel/vmm.c

test: ${TESTS}
	@for t in ${TESTS}; do $$t || exit 1; done

clean:
	rm -rf ${BUILD}

run: ${IMAGE}
	qemu-system-x86_64 -drive format=raw,file=${IMAGE}
