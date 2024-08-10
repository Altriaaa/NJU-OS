#include <am.h>
#include <klib.h>

// #ifndef KERNEL_H__
// #define KERNEL_H__

typedef struct cpu {
    int noff;
    int intena;
} cpu;

extern cpu cpus[];
#define mycpu (&cpus[cpu_current()])

#define panic_(...) \
    do { \
        printf("Panic: " __VA_ARGS__); \
        halt(1); \
    } while (0)

#define UNLOCKED  0
#define LOCKED    1

typedef struct {
    const char *name;
    int status;
    cpu *cpu;
} spinlock_xv6;

// #endif

#define spin_init(name_) \
    ((spinlock_xv6) { \
        .name = name_, \
        .status = UNLOCKED, \
        .cpu = NULL, \
    })
void spin_lock(spinlock_xv6 *lk);
void spin_unlock(spinlock_xv6 *lk);

