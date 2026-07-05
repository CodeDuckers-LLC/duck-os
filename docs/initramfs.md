# Initramfs

Kernel uses tiny built-in initramfs image for read-only files.

No directories yet. Every entry is flat file name plus raw file bytes.

## Image Layout

All integer fields are little-endian `u64`.

Header:

- magic: 4 bytes, ASCII `IRFS`
- file_count: `u64`

Then `file_count` entries follow.

Each entry:

- name_length: `u64`
- data_length: `u64`
- name bytes: `name_length` bytes, including trailing `\0`
- padding to 8-byte alignment
- file data: `data_length` bytes
- padding to 8-byte alignment

Constraints:

- file names are basenames only
- no directories
- no compression
- no checksums
- file data may contain any bytes

## Host Tool

`tools/mkinitramfs.py` builds image:

```sh
python3 tools/mkinitramfs.py build/initramfs.img initramfs/hello.txt initramfs/motd.txt
```

Kernel build embeds resulting `build/initramfs.img` into final ELF through `objcopy`.

## Kernel API

- `initramfs_init()`: parse embedded image
- `initramfs_list()`: print file list
- `initramfs_find(name)`: find file metadata by exact name
- `initramfs_read(file)`: return pointer to file bytes

## Shell

- `ls`: list embedded files
- `cat <file>`: print file contents
