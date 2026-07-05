#ifndef MM_PMM_H
#define MM_PMM_H

#define PMM_PAGE_SIZE 4096UL

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
unsigned long pmm_total_pages(void);
unsigned long pmm_free_pages(void);
unsigned long pmm_used_pages(void);

#endif
