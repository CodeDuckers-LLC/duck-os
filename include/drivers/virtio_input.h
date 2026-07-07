#ifndef DRIVERS_VIRTIO_INPUT_H
#define DRIVERS_VIRTIO_INPUT_H

void virtio_input_init(void);
int virtio_input_available(void);
void virtio_input_poll(void);

#endif
