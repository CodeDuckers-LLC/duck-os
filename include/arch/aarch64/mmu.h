#ifndef ARCH_AARCH64_MMU_H
#define ARCH_AARCH64_MMU_H

void mmu_init(void);
int mmu_is_enabled(void);
void mmu_map_user_code_page(unsigned long virtual_address, unsigned long physical_address);
void mmu_map_user_data_page(unsigned long virtual_address, unsigned long physical_address);
void mmu_sync_for_exec(void *address, unsigned long size);

#endif
