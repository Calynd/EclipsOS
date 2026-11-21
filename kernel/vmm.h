#ifndef UDK_VMM_H
#define UDK_VMM_H

#include <stddef.h>
#include <stdint.h>
#include "pmm.h"

#define VMM_WRITE 0x002ull
#define VMM_USER 0x004ull
#define VMM_WRITE_THROUGH 0x008ull
#define VMM_CACHE_DISABLE 0x010ull
#define VMM_NO_EXECUTE (1ull << 63)
#define VMM_FLAGS_MASK (VMM_WRITE | VMM_USER | VMM_WRITE_THROUGH | VMM_CACHE_DISABLE | VMM_NO_EXECUTE)

#define VMM_PAGE_SIZE 0x1000ull
#define VMM_LARGE_PAGE_SIZE 0x200000ull

#define VMM_OK 0
#define VMM_ERR_NO_MEMORY (-1)
#define VMM_ERR_EXISTS (-2)
#define VMM_ERR_NOT_MAPPED (-3)
#define VMM_ERR_INVALID (-4)
#define VMM_ERR_LARGE (-5)

struct vmm_ops {
    uint64_t (*alloc_frame)(void *context);
    void (*free_frame)(void *context, uint64_t physical);
    void *(*map_physical)(void *context, uint64_t physical);
    void (*flush)(void *context, uint64_t virtual_address);
    void *context;
};

struct vmm_space {
    uint64_t root;
    const struct vmm_ops *ops;
};

int vmm_space_init(struct vmm_space *space, const struct vmm_ops *ops);
void vmm_space_destroy(struct vmm_space *space);
int vmm_map(struct vmm_space *space, uint64_t virtual_address, uint64_t physical, uint64_t flags);
int vmm_map_large(struct vmm_space *space, uint64_t virtual_address, uint64_t physical, uint64_t flags);
int vmm_unmap(struct vmm_space *space, uint64_t virtual_address, uint64_t *physical);
int vmm_translate(const struct vmm_space *space, uint64_t virtual_address, uint64_t *physical, uint64_t *flags);

#endif
