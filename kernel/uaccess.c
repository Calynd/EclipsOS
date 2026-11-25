#include "uaccess.h"

int uaccess_copy_from_user(const struct vmm_space *space, void *destination, uint64_t source, size_t length)
{
    uint8_t *out = destination;

    if (length == 0)
        return 0;
    if (source >= UACCESS_USER_LIMIT || (uint64_t)length > UACCESS_USER_LIMIT - source)
        return -1;

    while (length > 0) {
        uint64_t physical;
        uint64_t flags;
        size_t chunk = VMM_PAGE_SIZE - (size_t)(source & (VMM_PAGE_SIZE - 1));
        const uint8_t *in;

        if (chunk > length)
            chunk = length;
        if (vmm_translate(space, source, &physical, &flags) != VMM_OK)
            return -1;
        if ((flags & VMM_USER) == 0)
            return -1;
        in = space->ops->map_physical(space->ops->context, physical);
        for (size_t i = 0; i < chunk; i++)
            out[i] = in[i];
        out += chunk;
        source += chunk;
        length -= chunk;
    }
    return 0;
}
