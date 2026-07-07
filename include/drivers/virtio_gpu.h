#ifndef DRIVERS_VIRTIO_GPU_H
#define DRIVERS_VIRTIO_GPU_H

#include "gfx/framebuffer.h"

void virtio_gpu_init(void);
int virtio_gpu_available(void);
unsigned int virtio_gpu_width(void);
unsigned int virtio_gpu_height(void);
const framebuffer_t *virtio_gpu_framebuffer(void);
int virtio_gpu_redraw_demo(void);
int virtio_gpu_flush(void);

#endif
