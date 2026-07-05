#ifndef KERNEL_MEMORY_LAYOUT_H
#define KERNEL_MEMORY_LAYOUT_H

unsigned long memory_layout_ram_start(void);
unsigned long memory_layout_ram_end(void);
unsigned long memory_layout_kernel_start(void);
unsigned long memory_layout_kernel_end(void);
unsigned long memory_layout_first_free_phys(void);
void memory_layout_set_first_free_phys(unsigned long address);
void memory_layout_print(void);

#endif
