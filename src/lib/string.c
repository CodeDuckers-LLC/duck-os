#include "lib/string.h"

void *memset(void *dest, int value, unsigned long count)
{
    unsigned char *bytes;
    unsigned char fill;
    unsigned long i;

    bytes = (unsigned char *)dest;
    fill = (unsigned char)value;

    for (i = 0; i < count; i++) {
        bytes[i] = fill;
    }

    return dest;
}

void *memcpy(void *dest, const void *src, unsigned long count)
{
    unsigned char *dest_bytes;
    const unsigned char *src_bytes;
    unsigned long i;

    dest_bytes = (unsigned char *)dest;
    src_bytes = (const unsigned char *)src;

    for (i = 0; i < count; i++) {
        dest_bytes[i] = src_bytes[i];
    }

    return dest;
}

int memcmp(const void *lhs, const void *rhs, unsigned long count)
{
    const unsigned char *lhs_bytes;
    const unsigned char *rhs_bytes;
    unsigned long i;

    lhs_bytes = (const unsigned char *)lhs;
    rhs_bytes = (const unsigned char *)rhs;

    for (i = 0; i < count; i++) {
        if (lhs_bytes[i] != rhs_bytes[i]) {
            return (int)lhs_bytes[i] - (int)rhs_bytes[i];
        }
    }

    return 0;
}

unsigned long strlen(const char *str)
{
    unsigned long length;

    length = 0;
    while (str[length] != '\0') {
        length++;
    }

    return length;
}

int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    return (int)(unsigned char)*lhs - (int)(unsigned char)*rhs;
}

unsigned long strlcpy(char *dest, const char *src, unsigned long size)
{
    unsigned long src_length;
    unsigned long i;

    src_length = strlen(src);

    if (size == 0) {
        return src_length;
    }

    for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    dest[i] = '\0';
    return src_length;
}
