#include "arch/aarch64/gic.h"
#include "drivers/virtio.h"
#include "kernel/klog.h"
#include "platform/platform.h"

#define VIRTIO_MMIO_MAGIC_VALUE 0x000UL
#define VIRTIO_MMIO_VERSION 0x004UL
#define VIRTIO_MMIO_DEVICE_ID 0x008UL
#define VIRTIO_MMIO_VENDOR_ID 0x00cUL
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010UL
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014UL
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020UL
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024UL
#define VIRTIO_MMIO_QUEUE_SEL 0x030UL
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034UL
#define VIRTIO_MMIO_QUEUE_NUM 0x038UL
#define VIRTIO_MMIO_QUEUE_READY 0x044UL
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050UL
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060UL
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064UL
#define VIRTIO_MMIO_STATUS 0x070UL
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080UL
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084UL
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090UL
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094UL
#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0UL
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4UL

static struct virtio_mmio_device virtio_devices[32];
static unsigned int virtio_device_total;

static void virtio_dmb(void)
{
    asm volatile("dmb ish" ::: "memory");
}

static unsigned int virtio_mmio_read_features(const struct virtio_mmio_device *device, unsigned int page)
{
    virtio_mmio_write32(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, page);
    return virtio_mmio_read32(device, VIRTIO_MMIO_DEVICE_FEATURES);
}

static void virtio_mmio_write_features(const struct virtio_mmio_device *device,
                                       unsigned int page,
                                       unsigned int value)
{
    virtio_mmio_write32(device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, page);
    virtio_mmio_write32(device, VIRTIO_MMIO_DRIVER_FEATURES, value);
}

static void virtio_mmio_probe_slot(unsigned int slot)
{
    struct virtio_mmio_device *device;
    unsigned long base;
    unsigned int magic;
    unsigned int version;
    unsigned int device_id;

    device = &virtio_devices[slot];
    base = platform_get_virtio_mmio_base() + (platform_get_virtio_mmio_stride() * slot);

    device->base = base;
    device->slot = slot;
    device->irq = platform_get_virtio_mmio_irq(slot);
    device->present = 0;
    device->initialized = 0;
    device->irq_handler = 0;
    device->driver_data = 0;

    magic = virtio_mmio_read32(device, VIRTIO_MMIO_MAGIC_VALUE);
    version = virtio_mmio_read32(device, VIRTIO_MMIO_VERSION);
    device_id = virtio_mmio_read32(device, VIRTIO_MMIO_DEVICE_ID);

    if (magic != VIRTIO_MMIO_MAGIC || version != VIRTIO_MMIO_VERSION_MODERN ||
        device_id == VIRTIO_DEVICE_ID_INVALID)
    {
        return;
    }

    device->present = 1;
    device->device_id = device_id;
    device->vendor_id = virtio_mmio_read32(device, VIRTIO_MMIO_VENDOR_ID);
    virtio_device_total++;

    kprintf("[INFO] virtio: slot %u irq %u device %u vendor 0x%x\n",
            slot,
            device->irq,
            device->device_id,
            device->vendor_id);
}

void virtio_init(void)
{
    unsigned int slot_count;
    unsigned int slot;

    virtio_device_total = 0;
    slot_count = platform_get_virtio_mmio_count();
    if (slot_count > (sizeof(virtio_devices) / sizeof(virtio_devices[0])))
    {
        slot_count = sizeof(virtio_devices) / sizeof(virtio_devices[0]);
    }

    for (slot = 0; slot < slot_count; slot++)
    {
        virtio_mmio_probe_slot(slot);
    }

    kprintf("[INFO] virtio: detected %u device(s)\n", virtio_device_total);
}

void virtio_enable_irqs(void)
{
    unsigned int slot;

    for (slot = 0; slot < platform_get_virtio_mmio_count(); slot++)
    {
        if (virtio_devices[slot].present && virtio_devices[slot].initialized)
        {
            gic_enable_irq(virtio_devices[slot].irq);
        }
    }
}

void virtio_handle_irq(unsigned int irq_id)
{
    unsigned int slot;
    struct virtio_mmio_device *device;
    unsigned int interrupt_status;

    for (slot = 0; slot < platform_get_virtio_mmio_count(); slot++)
    {
        device = &virtio_devices[slot];
        if (device->present && device->initialized && device->irq == irq_id)
        {
            interrupt_status = virtio_mmio_read32(device, VIRTIO_MMIO_INTERRUPT_STATUS);
            if (interrupt_status != 0)
            {
                virtio_mmio_write32(device, VIRTIO_MMIO_INTERRUPT_ACK, interrupt_status);
            }

            if (device->irq_handler != 0)
            {
                device->irq_handler(device, interrupt_status);
            }
            return;
        }
    }

    kprintf("[ERROR] virtio: unexpected IRQ %u\n", irq_id);
}

unsigned int virtio_device_count(void)
{
    return virtio_device_total;
}

struct virtio_mmio_device *virtio_find_device(unsigned int device_id, unsigned int instance_index)
{
    unsigned int slot;
    unsigned int matches;

    matches = 0;
    for (slot = 0; slot < platform_get_virtio_mmio_count(); slot++)
    {
        if (virtio_devices[slot].present && virtio_devices[slot].device_id == device_id)
        {
            if (matches == instance_index)
            {
                return &virtio_devices[slot];
            }
            matches++;
        }
    }

    return 0;
}

unsigned int virtio_mmio_read32(const struct virtio_mmio_device *device, unsigned long offset)
{
    volatile unsigned int *reg;

    reg = (volatile unsigned int *)(device->base + offset);
    virtio_dmb();
    return *reg;
}

void virtio_mmio_write32(const struct virtio_mmio_device *device, unsigned long offset, unsigned int value)
{
    volatile unsigned int *reg;

    reg = (volatile unsigned int *)(device->base + offset);
    *reg = value;
    virtio_dmb();
}

int virtio_mmio_begin_init(struct virtio_mmio_device *device)
{
    virtio_mmio_write32(device, VIRTIO_MMIO_STATUS, 0);
    if (virtio_mmio_read32(device, VIRTIO_MMIO_STATUS) != 0)
    {
        return -1;
    }

    device->status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    virtio_mmio_write32(device, VIRTIO_MMIO_STATUS, device->status);
    return 0;
}

int virtio_mmio_negotiate_features(struct virtio_mmio_device *device,
                                   unsigned int feature_page0,
                                   unsigned int feature_page1)
{
    unsigned int device_page0;
    unsigned int device_page1;

    device_page0 = virtio_mmio_read_features(device, 0);
    device_page1 = virtio_mmio_read_features(device, 1);

    if ((device_page1 & (1U << (VIRTIO_F_VERSION_1 - 32U))) == 0)
    {
        return -1;
    }

    device->driver_features[0] = feature_page0 & device_page0;
    device->driver_features[1] = feature_page1 & device_page1;
    device->driver_features[1] |= (1U << (VIRTIO_F_VERSION_1 - 32U));

    virtio_mmio_write_features(device, 0, device->driver_features[0]);
    virtio_mmio_write_features(device, 1, device->driver_features[1]);

    device->status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_mmio_write32(device, VIRTIO_MMIO_STATUS, device->status);

    if ((virtio_mmio_read32(device, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0)
    {
        return -1;
    }

    return 0;
}

int virtio_mmio_setup_queue(struct virtio_mmio_device *device,
                            unsigned int queue_index,
                            struct virtqueue *queue)
{
    unsigned int max_size;
    unsigned long desc_addr;
    unsigned long avail_addr;
    unsigned long used_addr;

    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_SEL, queue_index);
    max_size = virtio_mmio_read32(device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0 || queue->size > max_size)
    {
        return -1;
    }

    desc_addr = (unsigned long)queue->desc;
    avail_addr = (unsigned long)queue->avail;
    used_addr = (unsigned long)queue->used;

    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_NUM, queue->size);
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_DESC_LOW, (unsigned int)desc_addr);
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_DESC_HIGH, (unsigned int)(desc_addr >> 32));
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (unsigned int)avail_addr);
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (unsigned int)(avail_addr >> 32));
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_USED_LOW, (unsigned int)used_addr);
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_USED_HIGH, (unsigned int)(used_addr >> 32));
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_READY, 1);
    return 0;
}

void virtio_mmio_notify_queue(const struct virtio_mmio_device *device, unsigned int queue_index)
{
    virtio_mmio_write32(device, VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);
}

void virtio_mmio_finish_init(struct virtio_mmio_device *device)
{
    device->status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_mmio_write32(device, VIRTIO_MMIO_STATUS, device->status);
    device->initialized = 1;
}

void virtio_mmio_fail(struct virtio_mmio_device *device)
{
    device->status |= VIRTIO_STATUS_FAILED;
    virtio_mmio_write32(device, VIRTIO_MMIO_STATUS, device->status);
}

void virtio_mmio_set_irq_handler(struct virtio_mmio_device *device,
                                 virtio_irq_fn irq_handler,
                                 void *driver_data)
{
    device->irq_handler = irq_handler;
    device->driver_data = driver_data;
}
