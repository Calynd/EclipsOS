#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"

#define REGION 65536

static int failures;

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static uint32_t rng_state = 12345;

static uint32_t rng(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

int main(void)
{
    struct heap heap;
    uint8_t *region = aligned_alloc(4096, REGION);

    check("region", region != NULL);
    check("init", heap_init(&heap, region, REGION) == 0);
    check("consistent after init", heap_check(&heap) == 0);
    size_t initial_free = heap_free_bytes(&heap);
    check("nothing used after init", heap_used_bytes(&heap) == 0);

    check("zero size", heap_alloc(&heap, 0) == NULL);
    check("too large", heap_alloc(&heap, REGION * 2) == NULL);

    uint8_t *a = heap_alloc(&heap, 10);
    uint8_t *b = heap_alloc(&heap, 100);
    uint8_t *c = heap_alloc(&heap, 1000);
    check("allocated", a && b && c);
    check("aligned", ((uintptr_t)a % HEAP_ALIGNMENT) == 0 && ((uintptr_t)b % HEAP_ALIGNMENT) == 0 &&
                         ((uintptr_t)c % HEAP_ALIGNMENT) == 0);
    check("distinct and ordered", a < b && b < c);
    check("no overlap", a + 16 <= b && b + 112 <= c);

    memset(a, 0xAA, 10);
    memset(b, 0xBB, 100);
    memset(c, 0xCC, 1000);
    check("contents a", a[0] == 0xAA && a[9] == 0xAA);
    check("contents b", b[0] == 0xBB && b[99] == 0xBB);
    check("contents c", c[0] == 0xCC && c[999] == 0xCC);
    check("consistent after allocs", heap_check(&heap) == 0);
    check("used accounting", heap_used_bytes(&heap) == 16 + 112 + 1008);

    check("free b", heap_free(&heap, b) == 0);
    check("double free", heap_free(&heap, b) == -2);
    check("contents a survive", a[0] == 0xAA && a[9] == 0xAA);
    check("contents c survive", c[0] == 0xCC && c[999] == 0xCC);

    uint8_t *d = heap_alloc(&heap, 100);
    check("freed block reused", d == b);
    heap_free(&heap, d);
    check("free a", heap_free(&heap, a) == 0);
    uint8_t *e = heap_alloc(&heap, 120);
    check("coalesced blocks serve a larger request", e == a);
    heap_free(&heap, e);
    check("free c", heap_free(&heap, c) == 0);
    check("everything free again", heap_free_bytes(&heap) == initial_free);
    check("consistent after frees", heap_check(&heap) == 0);

    check("null pointer rejected", heap_free(&heap, NULL) == -1);
    check("outside pointer rejected", heap_free(&heap, region + REGION + 64) == -1);
    uint8_t *f = heap_alloc(&heap, 64);
    check("inside pointer rejected", heap_free(&heap, f + 16) == -1);
    heap_free(&heap, f);

    int count = 0;
    void *pointers[4096];
    while (count < 4096 && (pointers[count] = heap_alloc(&heap, 200)) != NULL)
        count++;
    check("exhaustion returns NULL", heap_alloc(&heap, 200) == NULL);
    check("exhausted heap is consistent", heap_check(&heap) == 0);
    for (int i = 0; i < count; i++)
        check("free all", heap_free(&heap, pointers[i]) == 0);
    check("recovered after exhaustion", heap_free_bytes(&heap) == initial_free && heap_check(&heap) == 0);

    struct {
        uint8_t *pointer;
        size_t size;
        uint8_t pattern;
    } live[64];
    memset(live, 0, sizeof live);
    for (int round = 0; round < 20000; round++) {
        unsigned int slot = rng() % 64;

        if (live[slot].pointer != NULL) {
            for (size_t i = 0; i < live[slot].size; i++) {
                if (live[slot].pointer[i] != live[slot].pattern) {
                    check("pattern intact", 0);
                    break;
                }
            }
            check("stress free", heap_free(&heap, live[slot].pointer) == 0);
            live[slot].pointer = NULL;
        } else {
            size_t size = 1 + rng() % 700;
            uint8_t *p = heap_alloc(&heap, size);

            if (p != NULL) {
                live[slot].pointer = p;
                live[slot].size = size;
                live[slot].pattern = (uint8_t)rng();
                memset(p, live[slot].pattern, size);
            }
        }
        if (round % 1000 == 0)
            check("stress consistency", heap_check(&heap) == 0);
    }
    for (int i = 0; i < 64; i++) {
        if (live[i].pointer != NULL)
            check("final free", heap_free(&heap, live[i].pointer) == 0);
    }
    check("stress ends with a whole heap", heap_free_bytes(&heap) == initial_free && heap_check(&heap) == 0);

    free(region);
    if (failures == 0)
        printf("test_heap: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
