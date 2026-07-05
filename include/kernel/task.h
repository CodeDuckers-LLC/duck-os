#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include "arch/aarch64/exceptions.h"

enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED,
};

struct task
{
    const char *name;
    unsigned long sp;
    void (*entry)(void *arg);
    void *arg;
    void *stack_base;
    unsigned long stack_size;
    enum task_state state;
};

void task_init(void);
struct task *task_create(const char *name, void (*entry)(void *arg), void *arg);
void task_yield(void);
unsigned long task_active_count(void);
unsigned long task_time_slice_ticks(void);
struct exception_trap_frame *task_schedule_from_exception(struct exception_trap_frame *frame,
                                                          int current_can_continue);
void task_run_preemptive_demo(void);
void arch_context_switch(unsigned long *old_sp, unsigned long *new_sp);

#endif
