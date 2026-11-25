#include <stdio.h>
#include <string.h>
#include "charbuf.h"

static int failures;

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

int main(void)
{
    struct charbuf b;
    char c = 0;

    memset(&b, 0, sizeof b);
    check("empty pop", !charbuf_pop(&b, &c));

    check("push a", charbuf_push(&b, 'a'));
    check("push b", charbuf_push(&b, 'b'));
    check("pop a", charbuf_pop(&b, &c) && c == 'a');
    check("pop b", charbuf_pop(&b, &c) && c == 'b');
    check("empty again", !charbuf_pop(&b, &c));

    int pushed = 0;
    while (charbuf_push(&b, (char)('A' + pushed % 26)))
        pushed++;
    check("capacity is size minus one", pushed == (int)CHARBUF_SIZE - 1);
    check("full rejects", !charbuf_push(&b, 'x'));

    for (int i = 0; i < pushed; i++) {
        int ok = charbuf_pop(&b, &c);
        check("fifo order", ok && c == (char)('A' + i % 26));
    }
    check("drained", !charbuf_pop(&b, &c));

    for (int round = 0; round < 1000; round++) {
        char in = (char)('a' + round % 26);
        check("wrap push", charbuf_push(&b, in));
        check("wrap pop", charbuf_pop(&b, &c) && c == in);
    }

    if (failures == 0)
        printf("test_charbuf: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
