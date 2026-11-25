#ifndef ECLIPS_UACCESS_H
#define ECLIPS_UACCESS_H

#include <stddef.h>
#include <stdint.h>
#include "vmm.h"

#define UACCESS_USER_LIMIT 0x0000800000000000ull

int uaccess_copy_from_user(const struct vmm_space *space, void *destination, uint64_t source, size_t length);

#endif
