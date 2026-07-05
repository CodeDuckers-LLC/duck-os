#ifndef KERNEL_INITRAMFS_H
#define KERNEL_INITRAMFS_H

struct initramfs_file
{
    const char *name;
    const unsigned char *data;
    unsigned long size;
};

void initramfs_init(void);
void initramfs_list(void);
const struct initramfs_file *initramfs_find(const char *name);
const unsigned char *initramfs_read(const struct initramfs_file *file);
unsigned long initramfs_file_count(void);

#endif
