#include "arch/aarch64/mmu.h"
#include "block/block_device.h"
#include "drivers/virtio.h"
#include "drivers/virtio_blk.h"
#include "kernel/klog.h"
#include "kernel/spinlock.h"
#include "lib/string.h"

#define VIRTIO_BLK_QUEUE_INDEX 0U
#define VIRTIO_BLK_QUEUE_SIZE 8U
#define VIRTIO_BLK_BLOCK_SIZE 512U

#define VIRTIO_BLK_T_IN 0U
#define VIRTIO_BLK_S_OK 0U

#define VIRTIO_BLK_CONFIG_CAPACITY_LOW 0x100UL
#define VIRTIO_BLK_CONFIG_CAPACITY_HIGH 0x104UL

struct virtio_blk_request_header
{
    unsigned int type;
    unsigned int reserved;
    unsigned long sector;
};

struct virtio_blk_state
{
    struct virtio_mmio_device *device;
    block_device_t block_device;
    struct virtqueue queue;
    spinlock_t lock;
    struct virtio_blk_request_header request_header;
    volatile unsigned char request_status;
    volatile int request_done;
    volatile int request_busy;
    unsigned int request_used_len;
    void *request_buffer;
    unsigned long request_block_count;
    unsigned short header_desc;
    unsigned short data_desc;
    unsigned short status_desc;
};

static struct virtio_blk_state virtio_blk_state;

static unsigned long virtio_blk_read_capacity(const struct virtio_mmio_device *device)
{
    unsigned long low;
    unsigned long high;

    low = virtio_mmio_read32(device, VIRTIO_BLK_CONFIG_CAPACITY_LOW);
    high = virtio_mmio_read32(device, VIRTIO_BLK_CONFIG_CAPACITY_HIGH);
    return low | (high << 32);
}

static void virtio_blk_free_pending_chain(void)
{
    virtqueue_free_desc(&virtio_blk_state.queue, virtio_blk_state.header_desc);
    virtqueue_free_desc(&virtio_blk_state.queue, virtio_blk_state.data_desc);
    virtqueue_free_desc(&virtio_blk_state.queue, virtio_blk_state.status_desc);
}

static void virtio_blk_complete_used(void)
{
    unsigned int head_index;
    unsigned int used_len;

    while (virtqueue_pop_used(&virtio_blk_state.queue, &head_index, &used_len))
    {
        if (!virtio_blk_state.request_busy ||
            head_index != virtio_blk_state.header_desc)
        {
            continue;
        }

        virtio_blk_free_pending_chain();
        virtio_blk_state.request_used_len = used_len;
        virtio_blk_state.request_done = 1;
        virtio_blk_state.request_busy = 0;
    }
}

static void virtio_blk_irq(struct virtio_mmio_device *device, unsigned int interrupt_status)
{
    (void)device;

    if ((interrupt_status & VIRTIO_INTERRUPT_CONFIG_CHANGE) != 0U)
    {
        kprintf("[INFO] virtio-blk: config change IRQ\n");
    }

    if ((interrupt_status & VIRTIO_INTERRUPT_USED_RING) == 0U)
    {
        return;
    }

    virtio_blk_complete_used();
}

static int virtio_blk_read_blocks_fn(struct block_device *device,
                                     unsigned long start_block,
                                     unsigned long block_count,
                                     void *buffer)
{
    unsigned long flags;
    unsigned long byte_count;
    int header_desc;
    int data_desc;
    int status_desc;

    if (device == 0 || buffer == 0 || block_count == 0)
    {
        return -1;
    }

    if (!virtio_blk_available())
    {
        return -1;
    }

    if (start_block >= device->block_count ||
        block_count > (device->block_count - start_block))
    {
        return -1;
    }

    byte_count = block_count * device->block_size;
    if (byte_count / device->block_size != block_count)
    {
        return -1;
    }

    flags = spin_lock_irqsave(&virtio_blk_state.lock);
    if (virtio_blk_state.request_busy)
    {
        spin_unlock_irqrestore(&virtio_blk_state.lock, flags);
        return -1;
    }

    header_desc = virtqueue_alloc_desc(&virtio_blk_state.queue);
    data_desc = virtqueue_alloc_desc(&virtio_blk_state.queue);
    status_desc = virtqueue_alloc_desc(&virtio_blk_state.queue);
    if (header_desc < 0 || data_desc < 0 || status_desc < 0)
    {
        if (status_desc >= 0)
        {
            virtqueue_free_desc(&virtio_blk_state.queue, (unsigned short)status_desc);
        }
        if (data_desc >= 0)
        {
            virtqueue_free_desc(&virtio_blk_state.queue, (unsigned short)data_desc);
        }
        if (header_desc >= 0)
        {
            virtqueue_free_desc(&virtio_blk_state.queue, (unsigned short)header_desc);
        }
        spin_unlock_irqrestore(&virtio_blk_state.lock, flags);
        return -1;
    }

    /* One synchronous request uses three descriptors:
     * header written by driver, data buffer written by device, status byte written by device.
     */
    virtio_blk_state.request_header.type = VIRTIO_BLK_T_IN;
    virtio_blk_state.request_header.reserved = 0;
    virtio_blk_state.request_header.sector = start_block;
    virtio_blk_state.request_status = 0xffU;
    virtio_blk_state.request_done = 0;
    virtio_blk_state.request_busy = 1;
    virtio_blk_state.request_used_len = 0;
    virtio_blk_state.request_buffer = buffer;
    virtio_blk_state.request_block_count = block_count;
    virtio_blk_state.header_desc = (unsigned short)header_desc;
    virtio_blk_state.data_desc = (unsigned short)data_desc;
    virtio_blk_state.status_desc = (unsigned short)status_desc;

    virtio_blk_state.queue.desc[header_desc].addr = (unsigned long)&virtio_blk_state.request_header;
    virtio_blk_state.queue.desc[header_desc].len = sizeof(virtio_blk_state.request_header);
    virtio_blk_state.queue.desc[header_desc].flags = VIRTQ_DESC_F_NEXT;
    virtio_blk_state.queue.desc[header_desc].next = (unsigned short)data_desc;

    virtio_blk_state.queue.desc[data_desc].addr = (unsigned long)buffer;
    virtio_blk_state.queue.desc[data_desc].len = (unsigned int)byte_count;
    virtio_blk_state.queue.desc[data_desc].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    virtio_blk_state.queue.desc[data_desc].next = (unsigned short)status_desc;

    virtio_blk_state.queue.desc[status_desc].addr = (unsigned long)&virtio_blk_state.request_status;
    virtio_blk_state.queue.desc[status_desc].len = sizeof(virtio_blk_state.request_status);
    virtio_blk_state.queue.desc[status_desc].flags = VIRTQ_DESC_F_WRITE;
    virtio_blk_state.queue.desc[status_desc].next = 0;

    mmu_sync_for_device(&virtio_blk_state.request_header, sizeof(virtio_blk_state.request_header));
    mmu_sync_for_device((void *)buffer, byte_count);
    mmu_sync_for_device((void *)&virtio_blk_state.request_status, sizeof(virtio_blk_state.request_status));
    virtqueue_submit(&virtio_blk_state.queue, (unsigned short)header_desc);
    virtio_mmio_notify_queue(virtio_blk_state.device, VIRTIO_BLK_QUEUE_INDEX);
    spin_unlock_irqrestore(&virtio_blk_state.lock, flags);

    while (!virtio_blk_state.request_done)
    {
        virtio_blk_complete_used();
    }

    mmu_sync_for_cpu((void *)buffer, byte_count);
    mmu_sync_for_cpu((void *)&virtio_blk_state.request_status, sizeof(virtio_blk_state.request_status));

    if (virtio_blk_state.request_status != VIRTIO_BLK_S_OK ||
        virtio_blk_state.request_used_len < 1U)
    {
        return -1;
    }

    return 0;
}

void virtio_blk_init(void)
{
    struct virtio_mmio_device *device;
    unsigned long capacity;

    memset(&virtio_blk_state, 0, sizeof(virtio_blk_state));
    virtio_blk_state.lock.value = 0;

    device = virtio_find_device(VIRTIO_DEVICE_ID_BLOCK, 0);
    if (device == 0)
    {
        kprintf("[INFO] virtio-blk: no device found\n");
        return;
    }

    if (virtio_mmio_begin_init(device) != 0)
    {
        kprintf("[ERROR] virtio-blk: reset/init failed\n");
        return;
    }

    /* Minimal feature set: accept only VERSION_1 required by modern transport. */
    if (virtio_mmio_negotiate_features(device, 0, 0) != 0)
    {
        kprintf("[ERROR] virtio-blk: feature negotiation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtqueue_init(&virtio_blk_state.queue, VIRTIO_BLK_QUEUE_SIZE) != 0)
    {
        kprintf("[ERROR] virtio-blk: queue allocation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtio_mmio_setup_queue(device, VIRTIO_BLK_QUEUE_INDEX, &virtio_blk_state.queue) != 0)
    {
        kprintf("[ERROR] virtio-blk: queue setup failed\n");
        virtio_mmio_fail(device);
        return;
    }

    capacity = virtio_blk_read_capacity(device);
    if (capacity == 0UL)
    {
        kprintf("[ERROR] virtio-blk: zero capacity\n");
        virtio_mmio_fail(device);
        return;
    }

    virtio_blk_state.block_device.name = "vda";
    virtio_blk_state.block_device.block_size = VIRTIO_BLK_BLOCK_SIZE;
    virtio_blk_state.block_device.block_count = capacity;
    virtio_blk_state.block_device.driver_data = &virtio_blk_state;
    virtio_blk_state.block_device.read_blocks = virtio_blk_read_blocks_fn;
    virtio_blk_state.block_device.write_blocks = 0;

    if (block_register_device(&virtio_blk_state.block_device) != 0)
    {
        kprintf("[ERROR] virtio-blk: block register failed\n");
        virtio_mmio_fail(device);
        return;
    }

    virtio_mmio_set_irq_handler(device, virtio_blk_irq, &virtio_blk_state);
    virtio_mmio_finish_init(device);
    virtio_blk_state.device = device;

    kprintf("[INFO] virtio-blk: ready as %s blocks=%u\n",
            virtio_blk_state.block_device.name,
            (unsigned int)virtio_blk_state.block_device.block_count);
}

int virtio_blk_available(void)
{
    return virtio_blk_state.device != 0 && virtio_blk_state.device->initialized;
}
