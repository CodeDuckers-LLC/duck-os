#include "arch/aarch64/sysreg.h"
#include "kernel/spinlock.h"

unsigned long irq_save(void)
{
    unsigned long flags;

    flags = sysreg_read_daif();
    sysreg_set_daif_irq();
    return flags;
}

void irq_restore(unsigned long flags)
{
    sysreg_write_daif(flags);
}

void spin_lock(spinlock_t *lock)
{
    unsigned int loaded;
    unsigned int stored;

    do
    {
        do
        {
            asm volatile("ldaxr %w0, [%1]"
                         : "=&r"(loaded)
                         : "r"(&lock->value)
                         : "memory");
        } while (loaded != 0U);

        stored = 1U;
        asm volatile("stxr %w0, %w2, [%1]"
                     : "=&r"(loaded)
                     : "r"(&lock->value), "r"(stored)
                     : "memory");
    } while (loaded != 0U);
}

void spin_unlock(spinlock_t *lock)
{
    asm volatile("stlr %w1, [%0]"
                 :
                 : "r"(&lock->value), "r"(0U)
                 : "memory");
}

unsigned long spin_lock_irqsave(spinlock_t *lock)
{
    unsigned long flags;

    flags = irq_save();
    spin_lock(lock);
    return flags;
}

void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    spin_unlock(lock);
    irq_restore(flags);
}
