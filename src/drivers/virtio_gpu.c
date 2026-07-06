#include "arch/aarch64/mmu.h"
#include "drivers/virtio.h"
#include "drivers/virtio_gpu.h"
#include "graphics/framebuffer.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "lib/string.h"

#define VIRTIO_GPU_QUEUE_CONTROL 0U
#define VIRTIO_GPU_QUEUE_SIZE 8U

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO 0x0100U
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101U
#define VIRTIO_GPU_CMD_SET_SCANOUT 0x0103U
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH 0x0104U
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105U
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106U

#define VIRTIO_GPU_RESP_OK_NODATA 0x1100U
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101U

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1U
#define VIRTIO_GPU_RESOURCE_ID 1U
#define VIRTIO_GPU_SCANOUT_ID 0U
#define VIRTIO_GPU_MAX_SCANOUTS 16U

struct virtio_gpu_ctrl_hdr
{
    unsigned int type;
    unsigned int flags;
    unsigned long fence_id;
    unsigned int ctx_id;
    unsigned int padding;
};

struct virtio_gpu_rect
{
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
};

struct virtio_gpu_display_one
{
    struct virtio_gpu_rect rect;
    unsigned int enabled;
    unsigned int flags;
};

struct virtio_gpu_resp_display_info
{
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

struct virtio_gpu_resource_create_2d
{
    struct virtio_gpu_ctrl_hdr hdr;
    unsigned int resource_id;
    unsigned int format;
    unsigned int width;
    unsigned int height;
};

struct virtio_gpu_resource_attach_backing
{
    struct virtio_gpu_ctrl_hdr hdr;
    unsigned int resource_id;
    unsigned int nr_entries;
};

struct virtio_gpu_mem_entry
{
    unsigned long addr;
    unsigned int length;
    unsigned int padding;
};

struct virtio_gpu_set_scanout
{
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect rect;
    unsigned int scanout_id;
    unsigned int resource_id;
};

struct virtio_gpu_transfer_to_host_2d
{
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect rect;
    unsigned long offset;
    unsigned int resource_id;
    unsigned int padding;
};

struct virtio_gpu_resource_flush
{
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect rect;
    unsigned int resource_id;
    unsigned int padding;
};

struct virtio_gpu_attach_backing_request
{
    struct virtio_gpu_resource_attach_backing attach;
    struct virtio_gpu_mem_entry entry;
};

struct virtio_gpu_state
{
    struct virtio_mmio_device *device;
    struct virtqueue controlq;
    framebuffer_t framebuffer;
    void *framebuffer_memory;
    unsigned int framebuffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int stride;
};

static struct virtio_gpu_state virtio_gpu_state;

static void virtio_gpu_init_header(struct virtio_gpu_ctrl_hdr *hdr, unsigned int type)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->type = type;
}

static int virtio_gpu_send_request(const void *request,
                                   unsigned int request_size,
                                   void *response,
                                   unsigned int response_size)
{
    unsigned int head_index;
    unsigned int used_len;
    int request_desc;
    int response_desc;

    if (virtio_gpu_state.device == 0 || request == 0 || response == 0)
    {
        return -1;
    }

    request_desc = virtqueue_alloc_desc(&virtio_gpu_state.controlq);
    response_desc = virtqueue_alloc_desc(&virtio_gpu_state.controlq);
    if (request_desc < 0 || response_desc < 0)
    {
        if (response_desc >= 0)
        {
            virtqueue_free_desc(&virtio_gpu_state.controlq, (unsigned short)response_desc);
        }
        if (request_desc >= 0)
        {
            virtqueue_free_desc(&virtio_gpu_state.controlq, (unsigned short)request_desc);
        }
        return -1;
    }

    virtio_gpu_state.controlq.desc[request_desc].addr = (unsigned long)request;
    virtio_gpu_state.controlq.desc[request_desc].len = request_size;
    virtio_gpu_state.controlq.desc[request_desc].flags = VIRTQ_DESC_F_NEXT;
    virtio_gpu_state.controlq.desc[request_desc].next = (unsigned short)response_desc;

    virtio_gpu_state.controlq.desc[response_desc].addr = (unsigned long)response;
    virtio_gpu_state.controlq.desc[response_desc].len = response_size;
    virtio_gpu_state.controlq.desc[response_desc].flags = VIRTQ_DESC_F_WRITE;
    virtio_gpu_state.controlq.desc[response_desc].next = 0;

    mmu_sync_for_device((void *)request, request_size);
    mmu_sync_for_device(response, response_size);
    virtqueue_submit(&virtio_gpu_state.controlq, (unsigned short)request_desc);
    virtio_mmio_notify_queue(virtio_gpu_state.device, VIRTIO_GPU_QUEUE_CONTROL);

    while (!virtqueue_pop_used(&virtio_gpu_state.controlq, &head_index, &used_len))
    {
    }

    mmu_sync_for_cpu(response, response_size);
    virtqueue_free_desc(&virtio_gpu_state.controlq, (unsigned short)response_desc);
    virtqueue_free_desc(&virtio_gpu_state.controlq, (unsigned short)request_desc);
    return 0;
}

static int virtio_gpu_simple_request(const void *request,
                                     unsigned int request_size,
                                     unsigned int expected_response_type)
{
    struct virtio_gpu_ctrl_hdr response;

    memset(&response, 0, sizeof(response));
    if (virtio_gpu_send_request(request, request_size, &response, sizeof(response)) != 0)
    {
        return -1;
    }

    return response.type == expected_response_type ? 0 : -1;
}

static int virtio_gpu_get_display_info(struct virtio_gpu_resp_display_info *response)
{
    struct virtio_gpu_ctrl_hdr request;

    virtio_gpu_init_header(&request, VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
    memset(response, 0, sizeof(*response));
    if (virtio_gpu_send_request(&request, sizeof(request), response, sizeof(*response)) != 0)
    {
        return -1;
    }

    return response->hdr.type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO ? 0 : -1;
}

static int virtio_gpu_create_resource(void)
{
    struct virtio_gpu_resource_create_2d request;

    memset(&request, 0, sizeof(request));
    virtio_gpu_init_header(&request.hdr, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
    request.resource_id = VIRTIO_GPU_RESOURCE_ID;
    request.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    request.width = virtio_gpu_state.width;
    request.height = virtio_gpu_state.height;
    return virtio_gpu_simple_request(&request, sizeof(request), VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_attach_backing(void)
{
    struct virtio_gpu_attach_backing_request request;

    memset(&request, 0, sizeof(request));
    virtio_gpu_init_header(&request.attach.hdr, VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
    request.attach.resource_id = VIRTIO_GPU_RESOURCE_ID;
    request.attach.nr_entries = 1U;
    request.entry.addr = (unsigned long)virtio_gpu_state.framebuffer_memory;
    request.entry.length = virtio_gpu_state.framebuffer_size;
    return virtio_gpu_simple_request(&request, sizeof(request), VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_set_scanout(void)
{
    struct virtio_gpu_set_scanout request;

    memset(&request, 0, sizeof(request));
    virtio_gpu_init_header(&request.hdr, VIRTIO_GPU_CMD_SET_SCANOUT);
    request.rect.width = virtio_gpu_state.width;
    request.rect.height = virtio_gpu_state.height;
    request.scanout_id = VIRTIO_GPU_SCANOUT_ID;
    request.resource_id = VIRTIO_GPU_RESOURCE_ID;
    return virtio_gpu_simple_request(&request, sizeof(request), VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_flush(void)
{
    struct virtio_gpu_transfer_to_host_2d transfer;
    struct virtio_gpu_resource_flush flush;

    if (!virtio_gpu_available())
    {
        return -1;
    }

    mmu_sync_for_device(virtio_gpu_state.framebuffer_memory, virtio_gpu_state.framebuffer_size);

    memset(&transfer, 0, sizeof(transfer));
    virtio_gpu_init_header(&transfer.hdr, VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
    transfer.rect.width = virtio_gpu_state.width;
    transfer.rect.height = virtio_gpu_state.height;
    transfer.resource_id = VIRTIO_GPU_RESOURCE_ID;
    if (virtio_gpu_simple_request(&transfer, sizeof(transfer), VIRTIO_GPU_RESP_OK_NODATA) != 0)
    {
        return -1;
    }

    memset(&flush, 0, sizeof(flush));
    virtio_gpu_init_header(&flush.hdr, VIRTIO_GPU_CMD_RESOURCE_FLUSH);
    flush.rect.width = virtio_gpu_state.width;
    flush.rect.height = virtio_gpu_state.height;
    flush.resource_id = VIRTIO_GPU_RESOURCE_ID;
    return virtio_gpu_simple_request(&flush, sizeof(flush), VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_redraw_demo(void)
{
    if (!virtio_gpu_available())
    {
        return -1;
    }

    framebuffer_draw_demo(&virtio_gpu_state.framebuffer);
    return virtio_gpu_flush();
}

void virtio_gpu_init(void)
{
    struct virtio_mmio_device *device;
    struct virtio_gpu_resp_display_info display_info;

    memset(&virtio_gpu_state, 0, sizeof(virtio_gpu_state));

    device = virtio_find_device(VIRTIO_DEVICE_ID_GPU, 0);
    if (device == 0)
    {
        kprintf("[INFO] virtio-gpu: no device found\n");
        return;
    }

    if (virtio_mmio_begin_init(device) != 0)
    {
        kprintf("[ERROR] virtio-gpu: reset/init failed\n");
        return;
    }

    if (virtio_mmio_negotiate_features(device, 0, 0) != 0)
    {
        kprintf("[ERROR] virtio-gpu: feature negotiation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtqueue_init(&virtio_gpu_state.controlq, VIRTIO_GPU_QUEUE_SIZE) != 0)
    {
        kprintf("[ERROR] virtio-gpu: control queue allocation failed\n");
        virtio_mmio_fail(device);
        return;
    }

    if (virtio_mmio_setup_queue(device, VIRTIO_GPU_QUEUE_CONTROL, &virtio_gpu_state.controlq) != 0)
    {
        kprintf("[ERROR] virtio-gpu: control queue setup failed\n");
        virtio_mmio_fail(device);
        return;
    }

    virtio_mmio_finish_init(device);
    virtio_gpu_state.device = device;

    if (virtio_gpu_get_display_info(&display_info) != 0)
    {
        kprintf("[ERROR] virtio-gpu: display info failed\n");
        virtio_gpu_state.device = 0;
        return;
    }

    virtio_gpu_state.width = display_info.pmodes[0].rect.width;
    virtio_gpu_state.height = display_info.pmodes[0].rect.height;
    if (virtio_gpu_state.width == 0U || virtio_gpu_state.height == 0U)
    {
        kprintf("[ERROR] virtio-gpu: invalid scanout size\n");
        virtio_gpu_state.device = 0;
        return;
    }

    virtio_gpu_state.stride = virtio_gpu_state.width * 4U;
    virtio_gpu_state.framebuffer_size = virtio_gpu_state.stride * virtio_gpu_state.height;
    virtio_gpu_state.framebuffer_memory = kzalloc(virtio_gpu_state.framebuffer_size);
    if (virtio_gpu_state.framebuffer_memory == 0)
    {
        kprintf("[ERROR] virtio-gpu: framebuffer allocation failed\n");
        virtio_gpu_state.device = 0;
        return;
    }

    framebuffer_init(&virtio_gpu_state.framebuffer,
                     virtio_gpu_state.width,
                     virtio_gpu_state.height,
                     virtio_gpu_state.stride,
                     4U,
                     virtio_gpu_state.framebuffer_memory);

    if (virtio_gpu_create_resource() != 0 ||
        virtio_gpu_attach_backing() != 0 ||
        virtio_gpu_set_scanout() != 0 ||
        virtio_gpu_redraw_demo() != 0)
    {
        kprintf("[ERROR] virtio-gpu: scanout setup failed\n");
        virtio_gpu_state.device = 0;
        return;
    }

    kprintf("[INFO] virtio-gpu: ready %ux%u on slot %u irq %u\n",
            virtio_gpu_state.width,
            virtio_gpu_state.height,
            device->slot,
            device->irq);
}

int virtio_gpu_available(void)
{
    return virtio_gpu_state.device != 0 && virtio_gpu_state.device->initialized;
}

unsigned int virtio_gpu_width(void)
{
    return virtio_gpu_state.width;
}

unsigned int virtio_gpu_height(void)
{
    return virtio_gpu_state.height;
}

const framebuffer_t *virtio_gpu_framebuffer(void)
{
    if (!virtio_gpu_available())
    {
        return 0;
    }

    return &virtio_gpu_state.framebuffer;
}
