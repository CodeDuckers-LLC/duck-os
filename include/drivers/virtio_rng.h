#ifndef DRIVERS_VIRTIO_RNG_H
#define DRIVERS_VIRTIO_RNG_H

void virtio_rng_init(void);
int virtio_rng_available(void);
int virtio_rng_fill(void *buffer, unsigned int size, unsigned int *bytes_written);

#endif
