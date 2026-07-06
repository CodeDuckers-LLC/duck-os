#include "arch/aarch64/mmu.h"
#include "drivers/virtqueue.h"
#include "lib/string.h"
#include "mm/pmm.h"

static void virtqueue_barrier(void)
{
    asm volatile("dmb ish" ::: "memory");
}

static unsigned long virtqueue_align_up(unsigned long value, unsigned long alignment)
{
    return (value + alignment - 1UL) & ~(alignment - 1UL);
}

static void virtqueue_sync_page_for_device(struct virtqueue *queue)
{
    mmu_sync_for_device(queue->page, PMM_PAGE_SIZE);
}

static void virtqueue_sync_page_for_cpu(struct virtqueue *queue)
{
    mmu_sync_for_cpu(queue->page, PMM_PAGE_SIZE);
}

int virtqueue_init(struct virtqueue *queue, unsigned short size)
{
    unsigned long desc_bytes;
    unsigned long avail_bytes;
    unsigned long used_offset;
    unsigned long used_bytes;
    unsigned long total_bytes;
    unsigned long page_address;

    if (queue == 0 || size == 0)
    {
        return -1;
    }

    desc_bytes = sizeof(struct virtq_desc) * size;
    avail_bytes = sizeof(struct virtq_avail) + (sizeof(unsigned short) * size) + sizeof(unsigned short);
    used_offset = virtqueue_align_up(desc_bytes + avail_bytes, 4UL);
    used_bytes = sizeof(struct virtq_used) + (sizeof(struct virtq_used_elem) * size) + sizeof(unsigned short);
    total_bytes = used_offset + used_bytes;

    if (total_bytes > PMM_PAGE_SIZE)
    {
        return -1;
    }

    page_address = (unsigned long)pmm_alloc_page();
    if (page_address == 0)
    {
        return -1;
    }

    memset((void *)page_address, 0, PMM_PAGE_SIZE);

    queue->size = size;
    queue->num_free = size;
    queue->free_head = 0;
    queue->last_used_idx = 0;
    queue->page = (void *)page_address;
    queue->desc = (struct virtq_desc *)page_address;
    queue->avail = (struct virtq_avail *)(page_address + desc_bytes);
    queue->avail_ring = (unsigned short *)(page_address + desc_bytes + sizeof(struct virtq_avail));
    queue->used = (struct virtq_used *)(page_address + used_offset);
    queue->used_ring = (struct virtq_used_elem *)(page_address + used_offset + sizeof(struct virtq_used));

    while (queue->free_head < (unsigned short)(size - 1U))
    {
        queue->desc[queue->free_head].next = queue->free_head + 1U;
        queue->free_head++;
    }

    queue->desc[queue->free_head].next = 0;
    queue->free_head = 0;
    return 0;
}

int virtqueue_alloc_desc(struct virtqueue *queue)
{
    unsigned short desc_index;

    if (queue == 0 || queue->num_free == 0)
    {
        return -1;
    }

    desc_index = queue->free_head;
    queue->free_head = queue->desc[desc_index].next;
    queue->num_free--;
    queue->desc[desc_index].next = 0;
    queue->desc[desc_index].flags = 0;
    return (int)desc_index;
}

void virtqueue_free_desc(struct virtqueue *queue, unsigned short desc_index)
{
    queue->desc[desc_index].next = queue->free_head;
    queue->desc[desc_index].flags = 0;
    queue->free_head = desc_index;
    queue->num_free++;
}

void virtqueue_submit(struct virtqueue *queue, unsigned short head_index)
{
    unsigned short ring_index;

    ring_index = (unsigned short)(queue->avail->idx % queue->size);
    queue->avail_ring[ring_index] = head_index;
    virtqueue_barrier();
    queue->avail->idx++;
    virtqueue_sync_page_for_device(queue);
    virtqueue_barrier();
}

int virtqueue_pop_used(struct virtqueue *queue, unsigned int *head_index, unsigned int *used_len)
{
    unsigned short used_index;
    struct virtq_used_elem *elem;

    virtqueue_sync_page_for_cpu(queue);
    virtqueue_barrier();

    if (queue->last_used_idx == queue->used->idx)
    {
        return 0;
    }

    used_index = (unsigned short)(queue->last_used_idx % queue->size);
    elem = &queue->used_ring[used_index];
    *head_index = elem->id;
    *used_len = elem->len;
    queue->last_used_idx++;
    return 1;
}
