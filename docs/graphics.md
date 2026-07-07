# Graphics Plan

Current implementation chooses **Option B: VirtIO GPU**, using the simple 2D path only.

## Comparison

### Fake framebuffer

- Simplest way to build drawing primitives
- No hardware dependency
- Good for testing rendering logic before scanout exists
- Does not display anything by itself

### Simple framebuffer

- Attractive on firmware-heavy platforms
- Not a natural fit for this bare-metal QEMU `virt` target
- Depends on a boot protocol or firmware-provided framebuffer contract

### VirtIO GPU

- Best long-term fit for QEMU `virt`
- Works with explicit QEMU display devices
- More driver work up front
- Lets the kernel keep a software-rendered 2D framebuffer while scanout is handled by the virtual GPU

## Staged Approach

1. Keep drawing code independent of the device in a software framebuffer library
2. Use that framebuffer for deterministic demo rendering
3. Attach the framebuffer to VirtIO GPU scanout 0
4. Keep the shell on serial first
5. Add a console backend abstraction later so UART and graphical console can share higher-level text rendering

## Current Files

- [include/gfx/framebuffer.h](../include/gfx/framebuffer.h)
- [src/gfx/framebuffer.c](../src/gfx/framebuffer.c)
- [include/drivers/virtio_gpu.h](/E:/Programming/Github/duck-os/include/drivers/virtio_gpu.h:1)
- [src/drivers/virtio_gpu.c](/E:/Programming/Github/duck-os/src/drivers/virtio_gpu.c:1)

## Interfaces

`framebuffer`:

- owns width, height, pitch, pixel size, pixel format, and memory pointer
- exposes test allocation plus clear, put-pixel, and get-pixel helpers

`virtio_gpu`:

- discovers device id `16`
- creates a 2D resource
- attaches guest backing memory
- binds scanout 0
- transfers and flushes framebuffer updates

## Console Direction

The shell remains UART-first for now.

Later, add a console abstraction with two backends:

- serial console backend using PL011
- graphical console backend using a character grid rendered into the software framebuffer

That split keeps command parsing and shell behavior independent from the output device.
