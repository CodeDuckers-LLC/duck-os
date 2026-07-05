#include "arch/aarch64/sysreg.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/panic.h"
#include "kernel/task.h"
#include "lib/string.h"

#define TASK_MAX 8
#define TASK_STACK_SIZE 16384UL
#define TASK_TIME_SLICE_TICKS 1UL

static struct task task_table[TASK_MAX];
static struct task kernel_task;
static struct task *current_task;
static int task_initialized;
static unsigned long task_demo_a;
static unsigned long task_demo_b;

static void task_trampoline(void);

static unsigned long align_down_16(unsigned long value)
{
    return value & ~15UL;
}

static int task_index_of(struct task *task)
{
    unsigned long i;

    if (task == &kernel_task)
    {
        return -1;
    }

    for (i = 0; i < TASK_MAX; i++)
    {
        if (&task_table[i] == task)
        {
            return (int)i;
        }
    }

    return -1;
}

static struct task *task_pick_next(void)
{
    int current_index;
    int i;

    current_index = task_index_of(current_task);
    for (i = 1; i <= TASK_MAX; i++)
    {
        int candidate_index;

        candidate_index = (current_index + i + TASK_MAX) % TASK_MAX;
        if (task_table[candidate_index].state == TASK_READY)
        {
            return &task_table[candidate_index];
        }
    }

    if (current_task != &kernel_task)
    {
        return &kernel_task;
    }

    return 0;
}

static void task_mark_finished(struct task *task)
{
    task->state = TASK_FINISHED;
}

static void task_prepare_stack(struct task *task)
{
    struct exception_trap_frame *frame;
    unsigned long sp;

    sp = (unsigned long)task->stack_base + task->stack_size;
    sp = align_down_16(sp);
    sp -= sizeof(*frame);

    frame = (struct exception_trap_frame *)sp;
    memset(frame, 0, sizeof(*frame));
    frame->elr_el1 = (unsigned long)task_trampoline;
    frame->spsr_el1 = (sysreg_read_daif() & 0x3c0UL) | 0x5UL;
    frame->sp = (unsigned long)task->stack_base + task->stack_size;

    task->sp = sp;
}

static void task_trampoline(void)
{
    current_task->entry(current_task->arg);
    task_mark_finished(current_task);
    task_yield();
    panic("task returned after finish");
}

void task_init(void)
{
    if (task_initialized)
    {
        return;
    }

    memset(task_table, 0, sizeof(task_table));
    memset(&kernel_task, 0, sizeof(kernel_task));

    kernel_task.name = "kernel";
    kernel_task.state = TASK_RUNNING;
    current_task = &kernel_task;
    task_initialized = 1;
}

struct task *task_create(const char *name, void (*entry)(void *arg), void *arg)
{
    unsigned long i;

    task_init();

    for (i = 0; i < TASK_MAX; i++)
    {
        struct task *task;

        task = &task_table[i];
        if (task->state != TASK_UNUSED)
        {
            continue;
        }

        task->stack_base = kmalloc(TASK_STACK_SIZE);
        if (task->stack_base == 0)
        {
            return 0;
        }

        task->name = name;
        task->entry = entry;
        task->arg = arg;
        task->stack_size = TASK_STACK_SIZE;
        task->state = TASK_READY;
        task_prepare_stack(task);
        return task;
    }

    return 0;
}

void task_yield(void)
{
    task_init();
    asm volatile("svc #0");
}

unsigned long task_active_count(void)
{
    unsigned long count;
    unsigned long i;

    task_init();

    count = 0;
    for (i = 0; i < TASK_MAX; i++)
    {
        if (task_table[i].state == TASK_READY || task_table[i].state == TASK_RUNNING)
        {
            count++;
        }
    }

    return count;
}

unsigned long task_time_slice_ticks(void)
{
    return TASK_TIME_SLICE_TICKS;
}

struct exception_trap_frame *task_schedule_from_exception(struct exception_trap_frame *frame,
                                                          int current_can_continue)
{
    struct task *previous_task;
    struct task *next_task;

    task_init();

    previous_task = current_task;
    previous_task->sp = (unsigned long)frame;

    next_task = task_pick_next();
    if (next_task == 0 || next_task == previous_task)
    {
        return frame;
    }

    if (current_can_continue && previous_task->state == TASK_RUNNING)
    {
        previous_task->state = TASK_READY;
    }

    next_task->state = TASK_RUNNING;
    current_task = next_task;
    return (struct exception_trap_frame *)next_task->sp;
}

struct exception_trap_frame *task_exit_from_exception(struct exception_trap_frame *frame,
                                                      unsigned long status)
{
    struct task *previous_task;
    struct task *next_task;

    (void)status;

    task_init();

    previous_task = current_task;
    previous_task->sp = (unsigned long)frame;

    if (previous_task == &kernel_task)
    {
        frame->x[0] = status;
        return frame;
    }

    task_mark_finished(previous_task);

    next_task = task_pick_next();
    if (next_task == 0)
    {
        kernel_task.state = TASK_RUNNING;
        current_task = &kernel_task;
        return (struct exception_trap_frame *)kernel_task.sp;
    }

    next_task->state = TASK_RUNNING;
    current_task = next_task;
    return (struct exception_trap_frame *)next_task->sp;
}

static void task_demo_preempt_a(void *arg)
{
    volatile unsigned long spin;

    (void)arg;

    for (task_demo_a = 0; task_demo_a < 3; task_demo_a++)
    {
        kprintf("[TASK] preempt A step %u\n", (unsigned int)(task_demo_a + 1));
        for (spin = 0; spin < 4000000UL; spin++)
        {
        }
    }
}

static void task_demo_preempt_b(void *arg)
{
    volatile unsigned long spin;

    (void)arg;

    for (task_demo_b = 0; task_demo_b < 3; task_demo_b++)
    {
        kprintf("[TASK] preempt B step %u\n", (unsigned int)(task_demo_b + 1));
        for (spin = 0; spin < 4000000UL; spin++)
        {
        }
    }
}

void task_run_preemptive_demo(void)
{
    struct task *task1;
    struct task *task2;

    task_demo_a = 0;
    task_demo_b = 0;

    task1 = task_create("preempt-a", task_demo_preempt_a, 0);
    task2 = task_create("preempt-b", task_demo_preempt_b, 0);

    if (task1 == 0 || task2 == 0)
    {
        panic("preemptive task demo create failed");
    }

    klog_info("scheduler: preemptive demo start");

    while (task_active_count() != 0)
    {
        task_yield();
    }

    if (task_demo_a != 3 || task_demo_b != 3)
    {
        panic("preemptive task demo failed");
    }

    klog_info("scheduler: preemptive demo done");
}
