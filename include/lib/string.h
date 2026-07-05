#ifndef LIB_STRING_H
#define LIB_STRING_H

unsigned long strlen(const char *str);

void *memset(void *dest, int value, unsigned long count);
void *memcpy(void *dest, const void *src, unsigned long count);
int memcmp(const void *lhs, const void *rhs, unsigned long count);

int strcmp(const char *lhs, const char *rhs);
unsigned long strlcpy(char *dest, const char *src, unsigned long size);

#endif
