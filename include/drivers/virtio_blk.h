#ifndef DRIVERS_VIRTIO_BLK_H
#define DRIVERS_VIRTIO_BLK_H

void virtio_blk_init(void);
int virtio_blk_available(void);
int virtio_blk_writable(void);

#endif
