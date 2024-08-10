// DO NOT MODIFY: Will be reverted by the Online Judge.

#include <kernel.h>
#include <klib.h>
#include <pthread.h>
#include <os.h>

void delay() {
    for (int volatile i = 0;
         i < 1000000; i++);
}

void foo(void* s)
{
    char* str = (char*)s;
    while(1)
    {
        putch(*str);
        // printf("    on CPU %d\n", cpu_current());
        delay();
    }
}

// void idle()
// {
//     iset(true);
//     while(1)
//     {
//         yield();
//     }
// }

sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define NPROD 2
#define NCONS 2

void T_produce(void *arg) { while (1) { P(&empty); putch('('); V(&fill);  } }
void T_consume(void *arg) { while (1) { P(&fill);  putch(')'); V(&empty); } }

static inline task_t *task_alloc() {
    return pmm->alloc(sizeof(task_t));
}

static void run_test1() {
    kmt->sem_init(&empty, "empty", 1);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        kmt->create(task_alloc(), "producer", T_produce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        kmt->create(task_alloc(), "consumer", T_consume, NULL);
    }
}


int main() {
    ioe_init();
    cte_init(os->trap);
    os->init();

    run_test1();

    // task_t* task1 = pmm->alloc(sizeof(task_t));
    // kmt->create(task1, "task1", foo, "a");
    // task_t* task2 = pmm->alloc(sizeof(task_t)); 
    // kmt->create(task2, "task2", foo, "b");
    // task_t* task3 = pmm->alloc(sizeof(task_t));
    // kmt->create(task3, "task3", foo, "c");
    // task_t* task4 = pmm->alloc(sizeof(task_t));
    // kmt->create(task4, "task4", foo, "d");
    // task_t* task5 = pmm->alloc(sizeof(task_t));
    // kmt->create(task5, "task5", foo, "e");
    // task_t* task6 = pmm->alloc(sizeof(task_t));
    // kmt->create(task6, "task6", foo, "f");
    // task_t* task7 = pmm->alloc(sizeof(task_t));
    // kmt->create(task7, "task7", foo, "g");
    // task_t* task8 = pmm->alloc(sizeof(task_t));
    // kmt->create(task8, "task8", foo, "h");
    // task_t* task9 = pmm->alloc(sizeof(task_t));
    // kmt->create(task9, "task9", foo, "i");
    // task_t* task10 = pmm->alloc(sizeof(task_t));
    // kmt->create(task10, "task10", foo, "j");
    // task_t* task11 = pmm->alloc(sizeof(task_t));
    // kmt->create(task11, "task11", foo, "k");
    // task_t* task12 = pmm->alloc(sizeof(task_t));
    // kmt->create(task12, "task12", foo, "l");
    // task_t* task13 = pmm->alloc(sizeof(task_t));
    // kmt->create(task13, "task13", foo, "m");

    mpe_init(os->run);
    return 1;
}