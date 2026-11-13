#ifndef RETRO_FREESTANDING_STRING_H
#define RETRO_FREESTANDING_STRING_H
#include <stddef.h>
void *memset(void *destination, int value, size_t count);
void *memcpy(void *destination, const void *source, size_t count);
#endif
