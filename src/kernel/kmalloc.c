#include "kernel/kmalloc.h"
#include "kernel/memory_layout.h"
#include "lib/string.h"

static unsigned long heap_start;
static unsigned long heap_current;
static unsigned long heap_limit;

static unsigned long align_up_16(unsigned long value)
{
    return (value + 15UL) & ~15UL;
}

static void kmalloc_init(void)
{
    if (heap_start != 0)
    {
        return;
    }

    heap_start = align_up_16(memory_layout_first_free_phys());
    heap_current = heap_start;
    heap_limit = memory_layout_ram_end();
}

void *kmalloc(unsigned long size)
{
    unsigned long aligned_size;
    unsigned long block_start;
    unsigned long block_end;

    kmalloc_init();

    aligned_size = align_up_16(size);
    block_start = heap_current;
    block_end = block_start + aligned_size;

    if (block_end < block_start || block_end > heap_limit)
    {
        return 0;
    }

    heap_current = block_end;
    return (void *)block_start;
}

void *kzalloc(unsigned long size)
{
    void *ptr;

    ptr = kmalloc(size);
    if (ptr == 0)
    {
        return 0;
    }

    memset(ptr, 0, size);
    return ptr;
}

unsigned long kmalloc_used(void)
{
    kmalloc_init();
    return heap_current - heap_start;
}
