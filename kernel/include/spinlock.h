#include <am.h>
#include <klib.h>

#ifndef KERNEL_H__
#define KERNEL_H__
struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];
#define mycpu (&cpus[cpu_current()])

#define panic_(...) \
    do { \
        printf("Panic: " __VA_ARGS__); \
        halt(1); \
    } while (0)

#endif

#define UNLOCKED  0
#define LOCKED    1

typedef struct {
    const char *name;
    int status;
    struct cpu *cpu;
} spinlock_t;

#define spin_init(name_) \
    ((spinlock_t) { \
        .name = name_, \
        .status = UNLOCKED, \
        .cpu = NULL, \
    })
void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);

