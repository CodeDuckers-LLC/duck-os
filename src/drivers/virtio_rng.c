#include "arch/aarch64/cpu.h"
#include "arch/aarch64/mmu.h"
#include "drivers/virtio.h"
#include "drivers/virtio_rng.h"
#include "kernel/klog.h"
#include "kernel/spinlock.h"

#define VIRTIO_RNG_QUEUE_INDEX 0U
#define VIRTIO_RNG_QUEUE_SIZE 8U

struct virtio_rng_state
{
    struct virtio_mmio_device *device;
    struct virtqueue queue;
    spinlock_t lock;
    volatile int request_done;
    volatile int request_busy;
    volatile unsigned int request_len;
    unsigned short pending_head;
};

static struct virtio_rng_state virtio_rng_state;

static void virtio_rng_irq(struct virtio_mmio_device *device, unsigned int interrupt_status)
{
    unsigned int head_index;
    unsigned int used_len;

    (void)device;

    if ((interrupt_status & VIRTIO_INTERRUPT_CONFIG_CHANGE) != 0)
    {
        kprintf("[INFO] virtio-rng: config change IRQ\n");
    }

    if ((interrupt_status & VIRTIO_INTERRUPT_USED_RING) == 0)
    {
        return;
    }

    while (virtqueue_pop_used(&virtio_rng_state.queue, &head_index, &used_len))
    {
        virtqueue_free_desc(&virtio_rng_state.queue, (unsigned short)head_index);
        virtio_rng_state.request_len = used_len;
        virtio_rng_state.request_done = 1;
        virtio_rng_state.request_busy = 0;
    }
}

void virtio_rng_init(void)
{
    struct virtio_mmio_device *device;

    device = virtio_find_device(VIRTIO_DEVICE_ID_RNG, 0);
    virtio_rng_state.device = 0;
    virtio_rng_state.lock.value = 0;
    virtio_rng_state.request_done = 0;
    virtio_rng_state.request_busy = 0;
    virtio_rng_state.request_len = 0;
    virtio_rng_state.pending_head = 0;

    if (device == 0)
    {
        kprintf("[INFO] virtio-rng: no device found\n");
        return;
    }

    if (virtio_mmio_begin_init(device) != 0)
    {
        kprintf("[ERROR] virtio-rng: reset/init failed\n");
        return;
    }

    if (virtio_mmio_negotiate_features(device, 0, 0) != 0)
    {
        kprintf("[ERROR] virtio-rng: feature negotiation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtqueue_init(&virtio_rng_state.queue, VIRTIO_RNG_QUEUE_SIZE) != 0)
    {
        kprintf("[ERROR] virtio-rng: queue allocation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtio_mmio_setup_queue(device, VIRTIO_RNG_QUEUE_INDEX, &virtio_rng_state.queue) != 0)
    {
        kprintf("[ERROR] virtio-rng: queue setup failed\n");
        virtio_mmio_fail(device);
        return;
    }

    virtio_mmio_set_irq_handler(device, virtio_rng_irq, &virtio_rng_state);
    virtio_mmio_finish_init(device);
    virtio_rng_state.device = device;

    kprintf("[INFO] virtio-rng: ready on slot %u irq %u\n", device->slot, device->irq);
}

int virtio_rng_available(void)
{
    return virtio_rng_state.device != 0 && virtio_rng_state.device->initialized;
}

int virtio_rng_fill(void *buffer, unsigned int size, unsigned int *bytes_written)
{
    unsigned long flags;
    int desc_index;

    if (bytes_written != 0)
    {
        *bytes_written = 0;
    }

    if (!virtio_rng_available() || buffer == 0 || size == 0)
    {
        return -1;
    }

    flags = spin_lock_irqsave(&virtio_rng_state.lock);
    if (virtio_rng_state.request_busy)
    {
        spin_unlock_irqrestore(&virtio_rng_state.lock, flags);
        return -1;
    }

    desc_index = virtqueue_alloc_desc(&virtio_rng_state.queue);
    if (desc_index < 0)
    {
        spin_unlock_irqrestore(&virtio_rng_state.lock, flags);
        return -1;
    }

    virtio_rng_state.queue.desc[desc_index].addr = (unsigned long)buffer;
    virtio_rng_state.queue.desc[desc_index].len = size;
    virtio_rng_state.queue.desc[desc_index].flags = VIRTQ_DESC_F_WRITE;
    virtio_rng_state.pending_head = (unsigned short)desc_index;
    virtio_rng_state.request_len = 0;
    virtio_rng_state.request_done = 0;
    virtio_rng_state.request_busy = 1;
    mmu_sync_for_device(buffer, size);
    virtqueue_submit(&virtio_rng_state.queue, (unsigned short)desc_index);
    virtio_mmio_notify_queue(virtio_rng_state.device, VIRTIO_RNG_QUEUE_INDEX);
    spin_unlock_irqrestore(&virtio_rng_state.lock, flags);

    while (!virtio_rng_state.request_done)
    {
        cpu_wait_for_interrupt();
    }

    mmu_sync_for_cpu(buffer, virtio_rng_state.request_len);

    if (bytes_written != 0)
    {
        *bytes_written = virtio_rng_state.request_len;
    }

    return 0;
}
