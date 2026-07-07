#include "arch/aarch64/mmu.h"
#include "drivers/virtio.h"
#include "drivers/virtio_input.h"
#include "drivers/virtio_gpu.h"
#include "gfx/cursor.h"
#include "kernel/input.h"
#include "kernel/klog.h"
#include "lib/string.h"

#define VIRTIO_INPUT_QUEUE_EVENT 0U
#define VIRTIO_INPUT_QUEUE_SIZE 32U
#define VIRTIO_INPUT_MAX_DEVICES 4U

#define VIRTIO_INPUT_EV_SYN 0U
#define VIRTIO_INPUT_EV_KEY 1U
#define VIRTIO_INPUT_EV_REL 2U
#define VIRTIO_INPUT_EV_ABS 3U

#define VIRTIO_INPUT_REL_X 0U
#define VIRTIO_INPUT_REL_Y 1U
#define VIRTIO_INPUT_ABS_X 0U
#define VIRTIO_INPUT_ABS_Y 1U
#define VIRTIO_INPUT_ABS_MAX 0x7fffU

struct virtio_input_event
{
    unsigned short type;
    unsigned short code;
    unsigned int value;
};

struct virtio_input_device_state
{
    struct virtio_mmio_device *device;
    struct virtqueue eventq;
    struct virtio_input_event events[VIRTIO_INPUT_QUEUE_SIZE];
    unsigned short desc_index[VIRTIO_INPUT_QUEUE_SIZE];
};

struct virtio_input_state
{
    struct virtio_input_device_state devices[VIRTIO_INPUT_MAX_DEVICES];
    unsigned int device_count;
};

static struct virtio_input_state virtio_input_state;

static void virtio_input_submit_buffer(struct virtio_input_device_state *state, unsigned int slot)
{
    mmu_sync_for_device(&state->events[slot], sizeof(state->events[slot]));
    virtqueue_submit(&state->eventq, state->desc_index[slot]);
}

static unsigned int virtio_input_scale_abs(unsigned int value, unsigned int size)
{
    if (size == 0U)
    {
        return 0U;
    }

    if (size == 1U)
    {
        return 0U;
    }

    if (value >= VIRTIO_INPUT_ABS_MAX)
    {
        return size - 1U;
    }

    return (unsigned int)(((unsigned long)value * (unsigned long)(size - 1U)) / VIRTIO_INPUT_ABS_MAX);
}

static void virtio_input_move_cursor_relative(int dx, int dy)
{
    framebuffer_t *fb;
    long x;
    long y;

    if (!gfx_cursor_available())
    {
        fb = (framebuffer_t *)virtio_gpu_framebuffer();
        if (fb == 0)
        {
            return;
        }
        gfx_cursor_attach(fb);
        gfx_cursor_move(fb->width / 2U, fb->height / 2U);
    }

    x = (long)gfx_cursor_x() + (long)dx;
    y = (long)gfx_cursor_y() + (long)dy;
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }

    gfx_cursor_move((unsigned int)x, (unsigned int)y);
    (void)virtio_gpu_flush();
}

static void virtio_input_move_cursor_absolute(unsigned short code, unsigned int value)
{
    framebuffer_t *fb;
    unsigned int x;
    unsigned int y;

    fb = (framebuffer_t *)virtio_gpu_framebuffer();
    if (fb == 0)
    {
        return;
    }

    if (!gfx_cursor_available())
    {
        gfx_cursor_attach(fb);
        gfx_cursor_move(fb->width / 2U, fb->height / 2U);
    }

    x = gfx_cursor_x();
    y = gfx_cursor_y();
    if (code == VIRTIO_INPUT_ABS_X)
    {
        x = virtio_input_scale_abs(value, fb->width);
    }
    else if (code == VIRTIO_INPUT_ABS_Y)
    {
        y = virtio_input_scale_abs(value, fb->height);
    }
    else
    {
        return;
    }

    gfx_cursor_move(x, y);
    (void)virtio_gpu_flush();
}

static void virtio_input_handle_event(const struct virtio_input_event *event)
{
    if (event->type == VIRTIO_INPUT_EV_KEY)
    {
        input_queue_key_event(event->code, event->value);
        return;
    }

    if (event->type == VIRTIO_INPUT_EV_REL)
    {
        int delta;

        delta = (int)event->value;
        if (event->code == VIRTIO_INPUT_REL_X)
        {
            virtio_input_move_cursor_relative(delta, 0);
        }
        else if (event->code == VIRTIO_INPUT_REL_Y)
        {
            virtio_input_move_cursor_relative(0, delta);
        }
        return;
    }

    if (event->type == VIRTIO_INPUT_EV_ABS)
    {
        virtio_input_move_cursor_absolute(event->code, event->value);
        return;
    }

    if (event->type == VIRTIO_INPUT_EV_SYN)
    {
        return;
    }
}

static void virtio_input_poll_device(struct virtio_input_device_state *state)
{
    unsigned int head_index;
    unsigned int used_len;
    unsigned int slot;

    while (virtqueue_pop_used(&state->eventq, &head_index, &used_len))
    {
        (void)used_len;

        for (slot = 0; slot < VIRTIO_INPUT_QUEUE_SIZE; slot++)
        {
            if (state->desc_index[slot] == (unsigned short)head_index)
            {
                mmu_sync_for_cpu(&state->events[slot], sizeof(state->events[slot]));
                virtio_input_handle_event(&state->events[slot]);
                virtio_input_submit_buffer(state, slot);
                break;
            }
        }
    }
}

void virtio_input_poll(void)
{
    unsigned int index;

    for (index = 0; index < virtio_input_state.device_count; index++)
    {
        virtio_input_poll_device(&virtio_input_state.devices[index]);
    }
}

static int virtio_input_init_device(struct virtio_input_device_state *state, struct virtio_mmio_device *device)
{
    unsigned int slot;

    memset(state, 0, sizeof(*state));

    if (virtio_mmio_begin_init(device) != 0)
    {
        kprintf("[ERROR] virtio-input: reset/init failed for slot %u\n", device->slot);
        return -1;
    }

    if (virtio_mmio_negotiate_features(device, 0, 0) != 0)
    {
        kprintf("[ERROR] virtio-input: feature negotiation failed for slot %u\n", device->slot);
        virtio_mmio_fail(device);
        return -1;
    }

    if (virtqueue_init(&state->eventq, VIRTIO_INPUT_QUEUE_SIZE) != 0)
    {
        kprintf("[ERROR] virtio-input: queue allocation failed for slot %u\n", device->slot);
        virtio_mmio_fail(device);
        return -1;
    }

    if (virtio_mmio_setup_queue(device, VIRTIO_INPUT_QUEUE_EVENT, &state->eventq) != 0)
    {
        kprintf("[ERROR] virtio-input: queue setup failed for slot %u\n", device->slot);
        virtio_mmio_fail(device);
        return -1;
    }

    for (slot = 0; slot < VIRTIO_INPUT_QUEUE_SIZE; slot++)
    {
        int desc_index;

        desc_index = virtqueue_alloc_desc(&state->eventq);
        if (desc_index < 0)
        {
            kprintf("[ERROR] virtio-input: descriptor allocation failed for slot %u\n", device->slot);
            virtio_mmio_fail(device);
            return -1;
        }

        state->desc_index[slot] = (unsigned short)desc_index;
        state->eventq.desc[desc_index].addr = (unsigned long)&state->events[slot];
        state->eventq.desc[desc_index].len = sizeof(state->events[slot]);
        state->eventq.desc[desc_index].flags = VIRTQ_DESC_F_WRITE;
        state->eventq.desc[desc_index].next = 0U;
        virtio_input_submit_buffer(state, slot);
    }

    virtio_mmio_finish_init(device);
    state->device = device;
    kprintf("[INFO] virtio-input: ready on slot %u irq %u\n", device->slot, device->irq);
    return 0;
}

void virtio_input_init(void)
{
    unsigned int instance;

    memset(&virtio_input_state, 0, sizeof(virtio_input_state));

    for (instance = 0; instance < VIRTIO_INPUT_MAX_DEVICES; instance++)
    {
        struct virtio_mmio_device *device;

        device = virtio_find_device(VIRTIO_DEVICE_ID_INPUT, instance);
        if (device == 0)
        {
            break;
        }

        if (virtio_input_init_device(&virtio_input_state.devices[virtio_input_state.device_count], device) == 0)
        {
            virtio_input_state.device_count++;
        }
    }

    if (virtio_input_state.device_count == 0U)
    {
        kprintf("[INFO] virtio-input: no device found\n");
    }
}

int virtio_input_available(void)
{
    return virtio_input_state.device_count != 0U;
}
