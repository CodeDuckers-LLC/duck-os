#include "arch/aarch64/mmu.h"
#include "drivers/virtio.h"
#include "drivers/virtio_input.h"
#include "kernel/input.h"
#include "kernel/klog.h"
#include "lib/string.h"

#define VIRTIO_INPUT_QUEUE_EVENT 0U
#define VIRTIO_INPUT_QUEUE_SIZE 32U

struct virtio_input_event
{
    unsigned short type;
    unsigned short code;
    unsigned int value;
};

struct virtio_input_state
{
    struct virtio_mmio_device *device;
    struct virtqueue eventq;
    struct virtio_input_event events[VIRTIO_INPUT_QUEUE_SIZE];
    unsigned short desc_index[VIRTIO_INPUT_QUEUE_SIZE];
};

static struct virtio_input_state virtio_input_state;

static void virtio_input_submit_buffer(unsigned int slot)
{
    mmu_sync_for_device(&virtio_input_state.events[slot], sizeof(virtio_input_state.events[slot]));
    virtqueue_submit(&virtio_input_state.eventq, virtio_input_state.desc_index[slot]);
}

void virtio_input_poll(void)
{
    unsigned int head_index;
    unsigned int used_len;
    unsigned int slot;

    if (!virtio_input_available())
    {
        return;
    }

    while (virtqueue_pop_used(&virtio_input_state.eventq, &head_index, &used_len))
    {
        (void)used_len;

        for (slot = 0; slot < VIRTIO_INPUT_QUEUE_SIZE; slot++)
        {
            if (virtio_input_state.desc_index[slot] == (unsigned short)head_index)
            {
                mmu_sync_for_cpu(&virtio_input_state.events[slot], sizeof(virtio_input_state.events[slot]));
                if (virtio_input_state.events[slot].type == INPUT_EVENT_KEY)
                {
                    input_queue_key_event(virtio_input_state.events[slot].code,
                                          virtio_input_state.events[slot].value);
                }
                virtio_input_submit_buffer(slot);
                break;
            }
        }
    }
}

void virtio_input_init(void)
{
    struct virtio_mmio_device *device;
    unsigned int slot;

    memset(&virtio_input_state, 0, sizeof(virtio_input_state));

    device = virtio_find_device(VIRTIO_DEVICE_ID_INPUT, 0);
    if (device == 0)
    {
        kprintf("[INFO] virtio-input: no device found\n");
        return;
    }

    if (virtio_mmio_begin_init(device) != 0)
    {
        kprintf("[ERROR] virtio-input: reset/init failed\n");
        return;
    }

    if (virtio_mmio_negotiate_features(device, 0, 0) != 0)
    {
        kprintf("[ERROR] virtio-input: feature negotiation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtqueue_init(&virtio_input_state.eventq, VIRTIO_INPUT_QUEUE_SIZE) != 0)
    {
        kprintf("[ERROR] virtio-input: queue allocation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtio_mmio_setup_queue(device, VIRTIO_INPUT_QUEUE_EVENT, &virtio_input_state.eventq) != 0)
    {
        kprintf("[ERROR] virtio-input: queue setup failed\n");
        virtio_mmio_fail(device);
        return;
    }

    for (slot = 0; slot < VIRTIO_INPUT_QUEUE_SIZE; slot++)
    {
        int desc_index;

        desc_index = virtqueue_alloc_desc(&virtio_input_state.eventq);
        if (desc_index < 0)
        {
            kprintf("[ERROR] virtio-input: descriptor allocation failed\n");
            virtio_mmio_fail(device);
            return;
        }

        virtio_input_state.desc_index[slot] = (unsigned short)desc_index;
        virtio_input_state.eventq.desc[desc_index].addr = (unsigned long)&virtio_input_state.events[slot];
        virtio_input_state.eventq.desc[desc_index].len = sizeof(virtio_input_state.events[slot]);
        virtio_input_state.eventq.desc[desc_index].flags = VIRTQ_DESC_F_WRITE;
        virtio_input_state.eventq.desc[desc_index].next = 0U;
        virtio_input_submit_buffer(slot);
    }

    virtio_mmio_finish_init(device);
    virtio_input_state.device = device;

    kprintf("[INFO] virtio-input: ready on slot %u irq %u\n", device->slot, device->irq);
}

int virtio_input_available(void)
{
    return virtio_input_state.device != 0 && virtio_input_state.device->initialized;
}
