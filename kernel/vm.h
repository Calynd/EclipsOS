#ifndef ECLIPS_VM_H
#define ECLIPS_VM_H

#include <stdint.h>
#include "vmm.h"

#define VM_TEST_ADDRESS 0x200000000ull

int vm_init(void);
struct vmm_space *vm_kernel_space(void);
int vm_self_test(void);

#endif
