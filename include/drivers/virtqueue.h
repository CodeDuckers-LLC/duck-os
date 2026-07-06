#ifndef DRIVERS_VIRTQUEUE_H
#define DRIVERS_VIRTQUEUE_H

#define VIRTQ_DESC_F_NEXT 1U
#define VIRTQ_DESC_F_WRITE 2U

struct virtq_desc
{
    unsigned long addr;
    unsigned int len;
    unsigned short flags;
    unsigned short next;
};

struct virtq_avail
{
    unsigned short flags;
    unsigned short idx;
};

struct virtq_used_elem
{
    unsigned int id;
    unsigned int len;
};

struct virtq_used
{
    unsigned short flags;
    unsigned short idx;
};

struct virtqueue
{
    unsigned short size;
    unsigned short num_free;
    unsigned short free_head;
    unsigned short last_used_idx;
    void *page;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    unsigned short *avail_ring;
    struct virtq_used *used;
    struct virtq_used_elem *used_ring;
};

int virtqueue_init(struct virtqueue *queue, unsigned short size);
int virtqueue_alloc_desc(struct virtqueue *queue);
void virtqueue_free_desc(struct virtqueue *queue, unsigned short desc_index);
void virtqueue_submit(struct virtqueue *queue, unsigned short head_index);
int virtqueue_pop_used(struct virtqueue *queue, unsigned int *head_index, unsigned int *used_len);

#endif
