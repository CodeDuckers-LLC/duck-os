#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

typedef struct
{
    volatile unsigned int value;
} spinlock_t;

unsigned long irq_save(void);
void irq_restore(unsigned long flags);

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

unsigned long spin_lock_irqsave(spinlock_t *lock);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

#endif
