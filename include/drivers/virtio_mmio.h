#ifndef DRIVERS_VIRTIO_MMIO_H
#define DRIVERS_VIRTIO_MMIO_H

/*
 * VirtIO MMIO register block used by QEMU `virt`.
 * Offsets come from VirtIO 1.x MMIO transport.
 */
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

struct virtio_mmio_device;
struct virtqueue;

unsigned int virtio_mmio_read32(const struct virtio_mmio_device *device, unsigned long offset);
void virtio_mmio_write32(const struct virtio_mmio_device *device, unsigned long offset, unsigned int value);
int virtio_mmio_begin_init(struct virtio_mmio_device *device);
int virtio_mmio_negotiate_features(struct virtio_mmio_device *device,
                                   unsigned int feature_page0,
                                   unsigned int feature_page1);
int virtio_mmio_setup_queue(struct virtio_mmio_device *device,
                            unsigned int queue_index,
                            struct virtqueue *queue);
void virtio_mmio_notify_queue(const struct virtio_mmio_device *device, unsigned int queue_index);
void virtio_mmio_finish_init(struct virtio_mmio_device *device);
void virtio_mmio_fail(struct virtio_mmio_device *device);

#endif
