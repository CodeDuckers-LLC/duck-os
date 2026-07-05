#include "kernel/console.h"
#include "kernel/syscall.h"
#include "kernel/task.h"
#include "kernel/timer.h"
#include "kernel/user.h"

static unsigned long syscall_error(void)
{
    return ~0UL;
}

unsigned long syscall_invoke(unsigned long number,
                             unsigned long arg0,
                             unsigned long arg1,
                             unsigned long arg2,
                             unsigned long arg3,
                             unsigned long arg4,
                             unsigned long arg5)
{
    register unsigned long x0 asm("x0") = arg0;
    register unsigned long x1 asm("x1") = arg1;
    register unsigned long x2 asm("x2") = arg2;
    register unsigned long x3 asm("x3") = arg3;
    register unsigned long x4 asm("x4") = arg4;
    register unsigned long x5 asm("x5") = arg5;
    register unsigned long x8 asm("x8") = number;

    asm volatile("svc #1"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                 : "memory");

    return x0;
}

unsigned long syscall_write_console(const char *buffer, unsigned long length)
{
    return syscall_invoke(SYS_WRITE_CONSOLE,
                          (unsigned long)buffer,
                          length,
                          0,
                          0,
                          0,
                          0);
}

unsigned long syscall_get_ticks(void)
{
    return syscall_invoke(SYS_GET_TICKS, 0, 0, 0, 0, 0, 0);
}

unsigned long syscall_exit(unsigned long status)
{
    return syscall_invoke(SYS_EXIT, status, 0, 0, 0, 0, 0);
}

struct exception_trap_frame *syscall_handle(struct exception_trap_frame *frame)
{
    switch (frame->x[8])
    {
    case SYS_WRITE_CONSOLE:
        if (frame->x[0] == 0)
        {
            frame->x[0] = syscall_error();
            return frame;
        }

        console_write_len((const char *)frame->x[0], frame->x[1]);
        frame->x[0] = frame->x[1];
        return frame;

    case SYS_GET_TICKS:
        frame->x[0] = timer_irq_count();
        return frame;

    case SYS_EXIT:
        if (user_exception_active())
        {
            return user_exit_from_exception(frame, frame->x[0]);
        }
        return task_exit_from_exception(frame, frame->x[0]);

    default:
        frame->x[0] = syscall_error();
        return frame;
    }
}
