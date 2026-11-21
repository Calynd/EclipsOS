#ifndef UDK_CHARBUF_H
#define UDK_CHARBUF_H

#include <stdint.h>

#define CHARBUF_SIZE 128u

struct charbuf {
    char data[CHARBUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
};

int charbuf_push(struct charbuf *buffer, char c);
int charbuf_pop(struct charbuf *buffer, char *c);

#endif
