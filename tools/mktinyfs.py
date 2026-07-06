#!/usr/bin/env python3

import os
import struct
import sys


MAGIC = 0x53465954
VERSION = 1
NAME_MAX = 64
BLOCK_SIZE = 512
MIN_IMAGE_SIZE = 4096
SUPERBLOCK_STRUCT = struct.Struct("<IIIII")
ENTRY_STRUCT = struct.Struct("<64sIII")


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def collect_files(input_dir):
    files = []
    names = []

    for root, _, filenames in os.walk(input_dir):
        for filename in filenames:
            path = os.path.join(root, filename)
            name = os.path.relpath(path, input_dir).replace(os.sep, "/")
            names.append((name, path))

    names.sort()

    for name, path in names:
        encoded = name.encode("utf-8")
        if len(encoded) >= NAME_MAX or b"\x00" in encoded:
            raise ValueError(f"bad file name: {name}")

        with open(path, "rb") as f:
            files.append((name, f.read()))

    return files


def build_image(files):
    file_table_offset = SUPERBLOCK_STRUCT.size
    file_table_size = len(files) * ENTRY_STRUCT.size
    data_offset = align_up(file_table_offset + file_table_size, BLOCK_SIZE)
    current_offset = data_offset

    entries = bytearray()
    data = bytearray()

    for name, contents in files:
        name_bytes = name.encode("utf-8")
        entry_name = name_bytes + (b"\x00" * (NAME_MAX - len(name_bytes)))

        entries += ENTRY_STRUCT.pack(entry_name, current_offset, len(contents), 0)
        data += contents

        current_offset += len(contents)
        padding = align_up(current_offset, BLOCK_SIZE) - current_offset
        data += b"\x00" * padding
        current_offset += padding

    image = bytearray()
    image += SUPERBLOCK_STRUCT.pack(MAGIC, VERSION, len(files), file_table_offset, data_offset)
    image += entries
    image += b"\x00" * (data_offset - len(image))
    image += data

    if len(image) % BLOCK_SIZE != 0:
        image += b"\x00" * (BLOCK_SIZE - (len(image) % BLOCK_SIZE))

    if len(image) < MIN_IMAGE_SIZE:
        image += b"\x00" * (MIN_IMAGE_SIZE - len(image))

    return bytes(image)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: mktinyfs.py OUTPUT INPUT_DIR", file=sys.stderr)
        return 1

    output = sys.argv[1]
    input_dir = sys.argv[2]

    try:
        files = collect_files(input_dir)
        image = build_image(files)
    except (OSError, ValueError) as exc:
        print(exc, file=sys.stderr)
        return 1

    with open(output, "wb") as f:
        f.write(image)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
