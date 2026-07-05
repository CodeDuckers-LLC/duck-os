#!/usr/bin/env python3

import os
import struct
import sys


MAGIC = b"IRFS"
ALIGN = 8


def align(data: bytes) -> bytes:
    padding = (-len(data)) % ALIGN
    return data + (b"\x00" * padding)


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: mkinitramfs.py OUTPUT FILE...", file=sys.stderr)
        return 1

    output = sys.argv[1]
    inputs = sys.argv[2:]

    image = bytearray()
    image += MAGIC
    image += struct.pack("<Q", len(inputs))

    for path in inputs:
        name = os.path.basename(path).encode("utf-8")
        data = open(path, "rb").read()

        if b"/" in name or b"\x00" in name:
            print(f"bad file name: {path}", file=sys.stderr)
            return 1

        image += struct.pack("<QQ", len(name) + 1, len(data))
        image += align(name + b"\x00")
        image += align(data)

    with open(output, "wb") as f:
        f.write(image)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
