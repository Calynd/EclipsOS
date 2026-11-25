#include "charbuf.h"

int charbuf_push(struct charbuf *buffer, char c)
{
    uint32_t next = (buffer->head + 1) & (CHARBUF_SIZE - 1);

    if (next == buffer->tail)
        return 0;
    buffer->data[buffer->head] = c;
    __asm__ volatile("" : : : "memory");
    buffer->head = next;
    return 1;
}

int charbuf_pop(struct charbuf *buffer, char *c)
{
    if (buffer->tail == buffer->head)
        return 0;
    *c = buffer->data[buffer->tail];
    __asm__ volatile("" : : : "memory");
    buffer->tail = (buffer->tail + 1) & (CHARBUF_SIZE - 1);
    return 1;
}
