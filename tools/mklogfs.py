#!/usr/bin/env python3

import struct
import sys


MAGIC = 0x5346474C
VERSION = 1
BLOCK_SIZE = 512
BLOCK_COUNT = 128
LOG_OFFSET = BLOCK_SIZE


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: mklogfs.py OUTPUT", file=sys.stderr)
        return 1

    image = bytearray(BLOCK_SIZE * BLOCK_COUNT)
    image[:20] = struct.pack("<IIIII", MAGIC, VERSION, BLOCK_SIZE, BLOCK_COUNT, LOG_OFFSET)

    with open(sys.argv[1], "wb") as f:
        f.write(image)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
