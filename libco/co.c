#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>

#define STACK_SIZE (1024 * 1024)
#define CO_MAX_NUM 128

enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
    const char *name;
    void (*func)(void *); // co_start 指定的入口地址和参数
    void *arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程
    ucontext_t     context;     // 协程的上下文
    uint8_t        stack[STACK_SIZE]; // 协程的栈空间
};

struct co* current = NULL;
struct co* all_co[CO_MAX_NUM] = {0};
long long co_num = 0;

__attribute__((constructor))
void co_init()
{
    current = (struct co*)malloc(sizeof(struct co));
    current->name = "main";
    current->func = NULL;
    current->arg = NULL;
    current->status = CO_RUNNING;
    current->waiter = NULL;
    all_co[0] = current;
    co_num = 1;
}

void coroutine_wrapper(void (*func)(void *), void *arg) {
    func(arg);
    current->status = CO_DEAD;
    if(current->waiter != NULL)
    {
        current->waiter->status = CO_RUNNING;
        struct co* prev = current;
        current = current->waiter;
        swapcontext(&prev->context, &current->context);
    }
    else 
        co_yield();
}

struct co *co_start(const char *name, void (*func)(void *), void *arg)
{
    //创建一个协程，但不立即执行
    struct co* co = (struct co*)malloc(sizeof(struct co));
    co->name = name;
    co->func = func;
    co->arg = arg;
    co->status = CO_NEW;
    co->waiter = NULL;
    all_co[co_num] = co;
    co_num++;

    getcontext(&co->context);
    co->context.uc_stack.ss_sp = co->stack;
    co->context.uc_stack.ss_size = STACK_SIZE;
    co->context.uc_stack.ss_flags = 0;
    co->context.uc_link = NULL;
    // co->context.uc_link = &(current->context);
    makecontext(&co->context, (void (*)(void))coroutine_wrapper, 2, func, arg);

    return co;
}

void co_wait(struct co *co)
{
    if(co == NULL)
    {
        return;
    }
    current->status = CO_WAITING;
    co->waiter = current;
    while(co->status != CO_DEAD)
    {
        co_yield();
    }
    free(co);
    for(int i = 0; i < co_num; i++)
    {
        if(all_co[i] == co)
        {
            all_co[i] = NULL;
            break;
        }
    }
}

void co_yield() 
{
    assert(current != NULL);
    // assert(current->status == CO_RUNNING || current->status == CO_WAITING);
    // assert(co_num > 0);

    int rand = random() % co_num;
    struct co* next = all_co[rand];
    while(next == NULL || next->status == CO_DEAD || next->status == CO_WAITING)
    {
        rand = random() % co_num;
        next = all_co[rand];
    }

    if(next->status == CO_NEW)
    {
        next->status = CO_RUNNING;
        // if(current->waiter) current = current->waiter;
    }
    struct co* prev = current;
    current = next;
    swapcontext(&prev->context, &next->context);
}

// int g_count = 0;

// static void add_count() {
//     g_count++;
// }

// static int get_count() {
//     return g_count;
// }

// static void work_1(void *arg) {
//     const char *s = (const char*)arg;
//     for (int i = 0; i < 100; ++i) {
//         printf("%s%d  ", s, get_count());
//         add_count();
//         co_yield();
//     }
// }

// static void work(void *arg) {

//     struct co* thd3 = co_start("thread-3", work_1, "Z");
//     co_wait(thd3);

//     const char *s = (const char*)arg;
//     for (int i = 0; i < 100; ++i) {
//         printf("%s%d  ", s, get_count());
//         add_count();
//         co_yield();
//     }
// }

// int main() {

//     struct co *thd1 = co_start("thread-1", work, "X");
//     struct co *thd2 = co_start("thread-2", work, "Y");

//     co_wait(thd1);
//     co_wait(thd2);
// }
