#ifndef DRIVERS_VIRTIO_H
#define DRIVERS_VIRTIO_H

#include "drivers/virtio_mmio.h"
#include "drivers/virtqueue.h"

#define VIRTIO_MMIO_MAGIC 0x74726976U
#define VIRTIO_MMIO_VERSION_MODERN 2U

#define VIRTIO_DEVICE_ID_INVALID 0U
#define VIRTIO_DEVICE_ID_BLOCK 2U
#define VIRTIO_DEVICE_ID_RNG 4U
#define VIRTIO_DEVICE_ID_GPU 16U
#define VIRTIO_DEVICE_ID_INPUT 18U

#define VIRTIO_STATUS_ACKNOWLEDGE 0x01U
#define VIRTIO_STATUS_DRIVER 0x02U
#define VIRTIO_STATUS_DRIVER_OK 0x04U
#define VIRTIO_STATUS_FEATURES_OK 0x08U
#define VIRTIO_STATUS_FAILED 0x80U

#define VIRTIO_INTERRUPT_USED_RING 0x01U
#define VIRTIO_INTERRUPT_CONFIG_CHANGE 0x02U

#define VIRTIO_F_VERSION_1 32U

struct virtio_mmio_device;

typedef void (*virtio_irq_fn)(struct virtio_mmio_device *device, unsigned int interrupt_status);

struct virtio_mmio_device
{
    unsigned long base;
    unsigned int slot;
    unsigned int irq;
    unsigned int device_id;
    unsigned int vendor_id;
    unsigned int status;
    unsigned int driver_features[2];
    int present;
    int initialized;
    virtio_irq_fn irq_handler;
    void *driver_data;
};

void virtio_init(void);
void virtio_enable_irqs(void);
void virtio_handle_irq(unsigned int irq_id);
unsigned int virtio_device_count(void);
struct virtio_mmio_device *virtio_find_device(unsigned int device_id, unsigned int instance_index);
void virtio_print_devices(void);
void virtio_mmio_set_irq_handler(struct virtio_mmio_device *device,
                                 virtio_irq_fn irq_handler,
                                 void *driver_data);

#endif
