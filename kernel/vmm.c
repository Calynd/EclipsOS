#include "vmm.h"

#define ENTRY_PRESENT 0x001ull
#define ENTRY_WRITE 0x002ull
#define ENTRY_USER 0x004ull
#define ENTRY_LARGE 0x080ull
#define ENTRY_ADDRESS 0x000FFFFFFFFFF000ull
#define ENTRY_ADDRESS_2M 0x000FFFFFFFE00000ull
#define ENTRY_ADDRESS_1G 0x000FFFFFC0000000ull
#define ENTRIES 512u

static uint64_t *table_at(const struct vmm_space *space, uint64_t physical)
{
    return space->ops->map_physical(space->ops->context, physical);
}

static int is_canonical(uint64_t address)
{
    uint64_t top = address >> 47;

    return top == 0 || top == 0x1FFFF;
}

static void flush(const struct vmm_space *space, uint64_t virtual_address)
{
    if (space->ops->flush != NULL)
        space->ops->flush(space->ops->context, virtual_address);
}

static uint64_t new_table(const struct vmm_space *space)
{
    uint64_t physical = space->ops->alloc_frame(space->ops->context);
    uint64_t *table;

    if (physical == PMM_NO_FRAME)
        return PMM_NO_FRAME;
    table = table_at(space, physical);
    for (unsigned int i = 0; i < ENTRIES; i++)
        table[i] = 0;
    return physical;
}

static int descend(const struct vmm_space *space, uint64_t *entry, int create, int user, uint64_t **next)
{
    if (!(*entry & ENTRY_PRESENT)) {
        uint64_t physical;

        if (!create)
            return VMM_ERR_NOT_MAPPED;
        physical = new_table(space);
        if (physical == PMM_NO_FRAME)
            return VMM_ERR_NO_MEMORY;
        *entry = physical | ENTRY_PRESENT | ENTRY_WRITE | (user ? ENTRY_USER : 0);
    } else if (*entry & ENTRY_LARGE) {
        return VMM_ERR_LARGE;
    } else if (user) {
        *entry |= ENTRY_USER;
    }
    *next = table_at(space, *entry & ENTRY_ADDRESS);
    return VMM_OK;
}

static int walk_to_pd(const struct vmm_space *space, uint64_t virtual_address, int create, int user, uint64_t **pd)
{
    uint64_t *table = table_at(space, space->root);
    int result;

    result = descend(space, &table[(virtual_address >> 39) & (ENTRIES - 1)], create, user, &table);
    if (result != VMM_OK)
        return result;
    result = descend(space, &table[(virtual_address >> 30) & (ENTRIES - 1)], create, user, &table);
    if (result != VMM_OK)
        return result;
    *pd = table;
    return VMM_OK;
}

int vmm_space_init(struct vmm_space *space, const struct vmm_ops *ops)
{
    uint64_t physical;

    space->ops = ops;
    space->root = PMM_NO_FRAME;
    physical = new_table(space);
    if (physical == PMM_NO_FRAME)
        return VMM_ERR_NO_MEMORY;
    space->root = physical;
    return VMM_OK;
}

static void free_tree(const struct vmm_space *space, uint64_t table_physical, int level)
{
    uint64_t *table = table_at(space, table_physical);

    if (level > 0) {
        for (unsigned int i = 0; i < ENTRIES; i++) {
            uint64_t entry = table[i];

            if (!(entry & ENTRY_PRESENT))
                continue;
            if ((entry & ENTRY_LARGE) && level <= 2)
                continue;
            free_tree(space, entry & ENTRY_ADDRESS, level - 1);
        }
    }
    space->ops->free_frame(space->ops->context, table_physical);
}

void vmm_space_destroy(struct vmm_space *space)
{
    if (space->root == PMM_NO_FRAME)
        return;
    free_tree(space, space->root, 3);
    space->root = PMM_NO_FRAME;
}

int vmm_map(struct vmm_space *space, uint64_t virtual_address, uint64_t physical, uint64_t flags)
{
    uint64_t *pd;
    uint64_t *pt;
    uint64_t *entry;
    int user = (flags & VMM_USER) != 0;
    int result;

    if (((virtual_address | physical) & (VMM_PAGE_SIZE - 1)) != 0)
        return VMM_ERR_INVALID;
    if (!is_canonical(virtual_address) || (physical & ~ENTRY_ADDRESS) != 0)
        return VMM_ERR_INVALID;
    if ((flags & ~VMM_FLAGS_MASK) != 0)
        return VMM_ERR_INVALID;

    result = walk_to_pd(space, virtual_address, 1, user, &pd);
    if (result == VMM_ERR_LARGE)
        return VMM_ERR_EXISTS;
    if (result != VMM_OK)
        return result;
    result = descend(space, &pd[(virtual_address >> 21) & (ENTRIES - 1)], 1, user, &pt);
    if (result == VMM_ERR_LARGE)
        return VMM_ERR_EXISTS;
    if (result != VMM_OK)
        return result;

    entry = &pt[(virtual_address >> 12) & (ENTRIES - 1)];
    if (*entry & ENTRY_PRESENT)
        return VMM_ERR_EXISTS;
    *entry = physical | flags | ENTRY_PRESENT;
    flush(space, virtual_address);
    return VMM_OK;
}

int vmm_map_large(struct vmm_space *space, uint64_t virtual_address, uint64_t physical, uint64_t flags)
{
    uint64_t *pd;
    uint64_t *entry;
    int user = (flags & VMM_USER) != 0;
    int result;

    if (((virtual_address | physical) & (VMM_LARGE_PAGE_SIZE - 1)) != 0)
        return VMM_ERR_INVALID;
    if (!is_canonical(virtual_address) || (physical & ~ENTRY_ADDRESS_2M) != 0)
        return VMM_ERR_INVALID;
    if ((flags & ~VMM_FLAGS_MASK) != 0)
        return VMM_ERR_INVALID;

    result = walk_to_pd(space, virtual_address, 1, user, &pd);
    if (result == VMM_ERR_LARGE)
        return VMM_ERR_EXISTS;
    if (result != VMM_OK)
        return result;

    entry = &pd[(virtual_address >> 21) & (ENTRIES - 1)];
    if (*entry & ENTRY_PRESENT)
        return VMM_ERR_EXISTS;
    *entry = physical | flags | ENTRY_PRESENT | ENTRY_LARGE;
    flush(space, virtual_address);
    return VMM_OK;
}

int vmm_unmap(struct vmm_space *space, uint64_t virtual_address, uint64_t *physical)
{
    uint64_t *pd;
    uint64_t *pt;
    uint64_t *entry;
    int result;

    if ((virtual_address & (VMM_PAGE_SIZE - 1)) != 0 || !is_canonical(virtual_address))
        return VMM_ERR_INVALID;

    result = walk_to_pd(space, virtual_address, 0, 0, &pd);
    if (result != VMM_OK)
        return result;
    result = descend(space, &pd[(virtual_address >> 21) & (ENTRIES - 1)], 0, 0, &pt);
    if (result != VMM_OK)
        return result;

    entry = &pt[(virtual_address >> 12) & (ENTRIES - 1)];
    if (!(*entry & ENTRY_PRESENT))
        return VMM_ERR_NOT_MAPPED;
    if (physical != NULL)
        *physical = *entry & ENTRY_ADDRESS;
    *entry = 0;
    flush(space, virtual_address);
    return VMM_OK;
}

int vmm_translate(const struct vmm_space *space, uint64_t virtual_address, uint64_t *physical, uint64_t *flags)
{
    uint64_t *table;
    uint64_t entry;
    uint64_t result_physical;

    if (!is_canonical(virtual_address))
        return VMM_ERR_INVALID;

    table = table_at(space, space->root);
    entry = table[(virtual_address >> 39) & (ENTRIES - 1)];
    if (!(entry & ENTRY_PRESENT))
        return VMM_ERR_NOT_MAPPED;

    table = table_at(space, entry & ENTRY_ADDRESS);
    entry = table[(virtual_address >> 30) & (ENTRIES - 1)];
    if (!(entry & ENTRY_PRESENT))
        return VMM_ERR_NOT_MAPPED;
    if (entry & ENTRY_LARGE) {
        result_physical = (entry & ENTRY_ADDRESS_1G) | (virtual_address & 0x3FFFFFFFull);
        goto found;
    }

    table = table_at(space, entry & ENTRY_ADDRESS);
    entry = table[(virtual_address >> 21) & (ENTRIES - 1)];
    if (!(entry & ENTRY_PRESENT))
        return VMM_ERR_NOT_MAPPED;
    if (entry & ENTRY_LARGE) {
        result_physical = (entry & ENTRY_ADDRESS_2M) | (virtual_address & 0x1FFFFFull);
        goto found;
    }

    table = table_at(space, entry & ENTRY_ADDRESS);
    entry = table[(virtual_address >> 12) & (ENTRIES - 1)];
    if (!(entry & ENTRY_PRESENT))
        return VMM_ERR_NOT_MAPPED;
    result_physical = (entry & ENTRY_ADDRESS) | (virtual_address & 0xFFFull);

found:
    if (physical != NULL)
        *physical = result_physical;
    if (flags != NULL)
        *flags = entry & VMM_FLAGS_MASK;
    return VMM_OK;
}
