#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include "arch/aarch64/exceptions.h"

#define USER_CODE_VA 0x00010000UL
#define USER_STACK_TOP 0x00200000UL

int user_run_demo(void);
int user_exception_active(void);
struct exception_trap_frame *user_exit_from_exception(struct exception_trap_frame *frame,
                                                      unsigned long status);

#endif
