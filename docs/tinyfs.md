# TinyFS

TinyFS is a tiny flat read-only filesystem for duck-os.

## Goals

- Keep the format obvious.
- Store a small set of named files.
- Support whole-file reads.
- Avoid directories, permissions, timestamps, and fragmentation.

## Layout

The image is laid out as:

1. superblock
2. fixed-size file table
3. file data

All file data is block-aligned to 512 bytes so the kernel can read files from block devices with simple block-sized I/O.

## Superblock

The superblock is 20 bytes and uses little-endian 32-bit fields:

- `magic`: `0x53465954` (`"TYFS"` in little-endian memory)
- `version`: filesystem format version, currently `1`
- `file_count`: number of file entries
- `file_table_offset`: byte offset of the file table
- `data_offset`: byte offset of the first file data region

## File Entry

Each file entry is 76 bytes:

- `name[64]`: null-padded file name
- `offset`: byte offset of file data from start of image
- `size`: file size in bytes
- `flags`: reserved, currently `0`

TinyFS currently supports only a flat namespace. File names must fit in 63 bytes plus a trailing zero byte.

## Tooling

Use `tools/mktinyfs.py` to pack a host directory into an image:

```sh
python3 tools/mktinyfs.py build/tinyfs.img rootfs
```

The builder sorts files by name, aligns each file's data to 512 bytes, and pads the final image to at least 4096 bytes.
