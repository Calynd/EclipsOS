#ifndef ECLIPS_USER_H
#define ECLIPS_USER_H

#include <stdint.h>

int user_run(void (*after_exit)(void));
__attribute__((noreturn)) void user_exit(int64_t code);

#endif
