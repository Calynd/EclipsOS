#ifndef ECLIPS_TIMER_H
#define ECLIPS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
uint32_t timer_frequency(void);

#endif
