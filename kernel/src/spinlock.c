#include <am.h>
#include <spinlock.h>

// This is a ported version of spin-lock
// from xv6-riscv to AbstractMachine:
// https://github.com/mit-pdos/xv6-riscv

void push_off_xv6();
void pop_off_xv6();
bool holding_xv6(spinlock_xv6 *lk);

void spin_lock(spinlock_xv6 *lk) {
    // printf("cpu %d acquiring %s\n", cpu_current(), lk->name);
    // Disable interrupts to avoid deadlock.
    push_off_xv6();

    // This is a deadlock.
    if (holding_xv6(lk)) {
        panic_("acquire %s", lk->name);
    }

    // This our main body of spin lock.
    int got;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
    } while (got != UNLOCKED);

    lk->cpu = mycpu;
    // printf("cpu %d acquired %s\n", cpu_current(), lk->name);
}

void spin_unlock(spinlock_xv6 *lk) {
    if (!holding_xv6(lk)) {
        panic_("release %s", lk->name);
    }

    lk->cpu = NULL;
    atomic_xchg(&lk->status, UNLOCKED);

    pop_off_xv6();
    // printf("cpu %d released %s\n", cpu_current(), lk->name);
    // sleep(1);
}

// Check whether this cpu is holding_xv6 the lock.
// Interrupts must be off.
bool holding_xv6(spinlock_xv6 *lk) {
    return (
        lk->status == LOCKED &&
        lk->cpu == &cpus[cpu_current()]
    );
}

// push_off_xv6/pop_off_xv6 are like intr_off()/intr_on()
// except that they are matched:
// it takes two pop_off_xv6()s to undo two push_off_xv6()s.
// Also, if interrupts are initially off, then
// push_off_xv6, pop_off_xv6 leaves them off.
void push_off_xv6(void) {
    int old = ienabled();
    cpu *c = mycpu;

    iset(false);
    if (c->noff == 0) {
        c->intena = old;
    }
    c->noff += 1;
}

void pop_off_xv6(void) {
    cpu *c = mycpu;

    // Never enable interrupt when holding_xv6 a lock.
    if (ienabled()) {
        panic_("pop_off_xv6 - interruptible");
    }
    
    if (c->noff < 1) {
        panic_("pop_off_xv6");
    }

    c->noff -= 1;
    if (c->noff == 0 && c->intena) {
        iset(true);
    }
}
