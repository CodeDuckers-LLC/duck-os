#ifndef KERNEL_KMALLOC_H
#define KERNEL_KMALLOC_H

void *kmalloc(unsigned long size);
void *kzalloc(unsigned long size);
unsigned long kmalloc_used(void);

#endif
