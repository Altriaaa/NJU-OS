// When we test your kernel implementation, the framework
// directory will be replaced. Any modifications you make
// to its files (e.g., kernel.h) will be lost. 

// Note that some code requires data structure definitions 
// (such as `sem_t`) to compile, and these definitions are
// not present in kernel.h. 

// Include these definitions in os.h.

#include <am.h>

typedef Context *(*handler_t)(Event, Context *);
typedef struct task task_t;
typedef struct spinlock spinlock_t;
typedef struct semaphore sem_t;

typedef struct cpu_intr
{
    int nest;
    int intena;
} cpu_intr_t;

typedef struct spinlock
{
    const char *name;
    int status;
    cpu_intr_t *cpu;
} spinlock_t;

typedef struct handler_list
{
    int seq;
    int event;
    handler_t handler;
    struct handler_list *next;
} handler_list_t;

typedef struct task
{
    const char* name;
    enum 
    {
        READY = 0,
        RUNNING,
        BLOCKED,   
        TERMINATED,
    } status;
    spinlock_t runnable_lock;  
    Context* ctx;
    task_t* pre;
    task_t* nxt;
    void* stk;
} task_t;

typedef struct wl_node
{
    task_t* task;
    struct wl_node* nxt;
    struct wl_node* pre;
} wl_node_t;

typedef struct semaphore
{
    int count;
    const char* name;
    spinlock_t lock;
    wl_node_t* wait_list;
} sem_t;

// #define TRACE_ENTRY printf("[trace] %s:entry\n", __func__)
// #define TRACE_EXIT printf("[trace] %s:exit\n", __func__)

#define TRACE_ENTRY ((void)0)
#define TRACE_EXIT ((void)0)