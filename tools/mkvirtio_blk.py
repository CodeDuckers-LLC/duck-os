#!/usr/bin/env python3

import pathlib
import sys


BLOCK_SIZE = 512
BLOCK_COUNT = 32


def build_image() -> bytes:
    image = bytearray(BLOCK_SIZE * BLOCK_COUNT)

    block0 = bytearray(BLOCK_SIZE)
    header = b"DUCKBLK0"
    footer = b"virtio-blk demo block 0\n"
    block0[0:len(header)] = header
    block0[16:16 + len(footer)] = footer
    for i in range(64, BLOCK_SIZE):
        block0[i] = i & 0xFF
    image[0:BLOCK_SIZE] = block0

    block1 = bytearray(BLOCK_SIZE)
    block1[0:16] = b"DUCKBLK1-SECOND"
    image[BLOCK_SIZE:2 * BLOCK_SIZE] = block1

    return bytes(image)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: mkvirtio_blk.py <output>", file=sys.stderr)
        return 1

    output = pathlib.Path(sys.argv[1])
    output.write_bytes(build_image())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
