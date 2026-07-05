#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "arch/aarch64/exceptions.h"

enum
{
    SYS_WRITE_CONSOLE = 1,
    SYS_GET_TICKS = 2,
    SYS_EXIT = 3,
};

/*
 * ARM64 syscall calling convention for this kernel:
 * - issue `svc #1`
 * - place syscall number in x8
 * - place arguments in x0-x5
 * - kernel returns result in x0
 *
 * `svc #0` remains reserved for internal cooperative task yield.
 */
unsigned long syscall_invoke(unsigned long number,
                             unsigned long arg0,
                             unsigned long arg1,
                             unsigned long arg2,
                             unsigned long arg3,
                             unsigned long arg4,
                             unsigned long arg5);

unsigned long syscall_write_console(const char *buffer, unsigned long length);
unsigned long syscall_get_ticks(void);
unsigned long syscall_exit(unsigned long status);

struct exception_trap_frame *syscall_handle(struct exception_trap_frame *frame);

#endif
