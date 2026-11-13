#include <string.h>

/* platform-local freestanding memory routines; shared code needs no host libc. */
void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes;
    bytes = (unsigned char *)destination;
    while (count) {
        *bytes++ = (unsigned char)value;
        --count;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *output;
    const unsigned char *input;
    output = (unsigned char *)destination;
    input = (const unsigned char *)source;
    while (count) {
        *output++ = *input++;
        --count;
    }
    return destination;
}

void __aeabi_memclr(void *destination, size_t count) { memset(destination, 0, count); }
void __aeabi_memclr4(void *destination, size_t count) { memset(destination, 0, count); }
void __aeabi_memclr8(void *destination, size_t count) { memset(destination, 0, count); }
void __aeabi_memcpy(void *destination, const void *source, size_t count) { memcpy(destination, source, count); }
void __aeabi_memcpy4(void *destination, const void *source, size_t count) { memcpy(destination, source, count); }
void __aeabi_memcpy8(void *destination, const void *source, size_t count) { memcpy(destination, source, count); }
