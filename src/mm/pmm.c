#include "kernel/klog.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "platform/platform.h"

static unsigned long *pmm_bitmap;
static unsigned long pmm_bitmap_bytes;
static unsigned long pmm_page_count;
static unsigned long pmm_used_count;
static int pmm_initialized;

static unsigned long align_up_page(unsigned long value)
{
    return (value + (PMM_PAGE_SIZE - 1)) & ~(PMM_PAGE_SIZE - 1);
}

static unsigned long align_down_page(unsigned long value)
{
    return value & ~(PMM_PAGE_SIZE - 1);
}

static unsigned long pmm_word_bits(void)
{
    return sizeof(unsigned long) * 8UL;
}

static int pmm_test_bit(unsigned long page_index)
{
    unsigned long word_index;
    unsigned long bit_index;

    word_index = page_index / pmm_word_bits();
    bit_index = page_index % pmm_word_bits();

    return (pmm_bitmap[word_index] & (1UL << bit_index)) != 0;
}

static void pmm_set_bit(unsigned long page_index)
{
    unsigned long word_index;
    unsigned long bit_index;

    word_index = page_index / pmm_word_bits();
    bit_index = page_index % pmm_word_bits();
    pmm_bitmap[word_index] |= (1UL << bit_index);
}

static void pmm_clear_bit(unsigned long page_index)
{
    unsigned long word_index;
    unsigned long bit_index;

    word_index = page_index / pmm_word_bits();
    bit_index = page_index % pmm_word_bits();
    pmm_bitmap[word_index] &= ~(1UL << bit_index);
}

static void pmm_reserve_page(unsigned long page_index)
{
    if (!pmm_test_bit(page_index))
    {
        pmm_set_bit(page_index);
        pmm_used_count++;
    }
}

static void pmm_reserve_range(unsigned long start, unsigned long end)
{
    unsigned long current;
    unsigned long ram_base;

    if (end <= start)
    {
        return;
    }

    ram_base = memory_layout_ram_start();
    current = align_down_page(start);
    end = align_up_page(end);

    while (current < end)
    {
        unsigned long page_index;

        page_index = (current - ram_base) / PMM_PAGE_SIZE;
        pmm_reserve_page(page_index);
        current += PMM_PAGE_SIZE;
    }
}

void pmm_init(void)
{
    unsigned long bitmap_start;
    unsigned long bitmap_end;
    unsigned long kernel_start;
    unsigned long kernel_end;

    if (pmm_initialized)
    {
        return;
    }

    pmm_page_count = platform_get_ram_size() / PMM_PAGE_SIZE;
    pmm_bitmap_bytes = (pmm_page_count + 7UL) / 8UL;

    bitmap_start = align_up_page(memory_layout_first_free_phys());
    pmm_bitmap_bytes = align_up_page(pmm_bitmap_bytes);
    bitmap_end = bitmap_start + pmm_bitmap_bytes;

    pmm_bitmap = (unsigned long *)bitmap_start;
    memset(pmm_bitmap, 0, pmm_bitmap_bytes);

    kernel_start = memory_layout_kernel_start();
    kernel_end = memory_layout_kernel_end();

    pmm_reserve_range(kernel_start, kernel_end);
    pmm_reserve_range(bitmap_start, bitmap_end);

    memory_layout_set_first_free_phys(bitmap_end);
    pmm_initialized = 1;

    kprintf("[INFO] pmm: total pages: %u\n", (unsigned int)pmm_page_count);
    kprintf("[INFO] pmm: used pages: %u\n", (unsigned int)pmm_used_count);
    kprintf("[INFO] pmm: free pages: %u\n",
            (unsigned int)(pmm_page_count - pmm_used_count));
}

void *pmm_alloc_page(void)
{
    unsigned long page_index;
    unsigned long ram_base;

    pmm_init();
    ram_base = memory_layout_ram_start();

    for (page_index = 0; page_index < pmm_page_count; page_index++)
    {
        if (!pmm_test_bit(page_index))
        {
            pmm_set_bit(page_index);
            pmm_used_count++;
            return (void *)(ram_base + (page_index * PMM_PAGE_SIZE));
        }
    }

    return 0;
}

void pmm_free_page(void *page)
{
    unsigned long address;
    unsigned long ram_base;
    unsigned long ram_end;
    unsigned long page_index;

    pmm_init();

    address = (unsigned long)page;
    ram_base = memory_layout_ram_start();
    ram_end = memory_layout_ram_end();

    if (address < ram_base || address >= ram_end)
    {
        panic("pmm_free_page: address out of range");
    }

    if ((address & (PMM_PAGE_SIZE - 1)) != 0)
    {
        panic("pmm_free_page: address not page aligned");
    }

    page_index = (address - ram_base) / PMM_PAGE_SIZE;
    if (!pmm_test_bit(page_index))
    {
        panic("pmm_free_page: page already free");
    }

    pmm_clear_bit(page_index);
    pmm_used_count--;
}

unsigned long pmm_total_pages(void)
{
    pmm_init();
    return pmm_page_count;
}

unsigned long pmm_free_pages(void)
{
    pmm_init();
    return pmm_page_count - pmm_used_count;
}

unsigned long pmm_used_pages(void)
{
    pmm_init();
    return pmm_used_count;
}
