#ifndef FS_FILE_H
#define FS_FILE_H

typedef struct file
{
    const char *path;
    unsigned int size;
    unsigned int offset;
    unsigned int flags;
    int in_use;
} file_t;

file_t *file_open(const char *path);
int file_read(file_t *file, void *buffer, unsigned int size);
int file_seek(file_t *file, unsigned int offset);
void file_close(file_t *file);

#endif
