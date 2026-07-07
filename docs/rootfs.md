# RootFS Workflow

`rootfs/` is the source tree for the TinyFS image used by duck-os.

## Quick Start

Rebuild the TinyFS image only:

```sh
make rootfs
```

Build the full project, including the kernel and TinyFS image:

```sh
make
```

## Layout

- `rootfs/etc/` holds simple system text files such as `motd` and `hostname`.
- `rootfs/home/` is a convenient place for user-visible sample files.
- `rootfs/bin/` holds checked-in placeholder files and receives generated binaries during the build.

TinyFS stores file names as strings, so paths like `etc/motd` and `home/readme.txt` are preserved in the image even though the on-disk format stays simple.

## Build Behavior

`make rootfs` does this:

1. copies `rootfs/` into `build/rootfs/`
2. stages generated binaries into `build/rootfs/bin/`
3. packs `build/tinyfs.img` with `tools/mktinyfs.py`

Any added, removed, or modified file under `rootfs/` is treated as an input to `build/tinyfs.img`, so rerunning `make rootfs` on Ubuntu is enough to pick up changes.
