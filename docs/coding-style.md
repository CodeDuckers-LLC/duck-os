# Coding Style

Keep style small, boring, easy to read.

## General

- Prefer clear code over clever code.
- Match existing file layout unless there is strong reason to change it.
- Keep early boot code simple and explicit.
- Avoid dependencies on hosted C runtime or standard library behavior.

## C

- Use freestanding-friendly C.
- Keep functions short when practical.
- Use `snake_case` for functions and variables.
- Use uppercase names for hardware base addresses and register macros.
- Mark MMIO registers as `volatile`.
- Use fixed-width integer types when register width matters.
- Do not hide important control flow in macros.

## Assembly

- Keep assembly limited to boot, low-level CPU setup, and exception entry.
- Move policy and higher-level logic into C as soon as possible.
- Comment non-obvious register use and calling assumptions.

## Logging And Errors

- Use `klog_info` and `klog_error` for simple boot diagnostics.
- Use `kprintf` when formatted output is actually needed.
- Keep `kprintf` usage within the supported specifier set: `%s`, `%c`, `%d`, `%u`, `%x`, `%p`, `%%`.
- Use `panic` for unrecoverable paths.
- Keep log strings short and specific.

## Layout

- One major responsibility per source file.
- Put public declarations in headers.
- Include only headers needed by file.

## Comments

- Write comments for hardware assumptions, memory layout, and tricky control flow.
- Do not write comments that repeat code word-for-word.

## Future Changes

When style changes, update this document with actual project conventions instead of aspirational ones.
