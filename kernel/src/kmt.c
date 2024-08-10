#include <common.h>
#include <os.h>

#define KMT_STACK_SIZE 8192
#define NCPU 16
#define UNLOCKED 0
#define LOCKED 1
#define INT_MIN -0x7fffffff - 1
#define INT_MAX 0x7fffffff

//declaration
static void kmt_init();
static int  kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg);
static void kmt_teardown(task_t *task);
static void kmt_spin_init(spinlock_t *lk, const char *name);
static void kmt_spin_lock(spinlock_t *lk);
static void kmt_spin_unlock(spinlock_t *lk);
static void kmt_sem_init(sem_t *sem, const char *name, int value);
static void kmt_sem_wait(sem_t *sem);
static void kmt_sem_signal(sem_t *sem);

task_t* task_pool;
task_t* idle[NCPU];
task_t* curr[NCPU];
task_t* last[NCPU];

#define IDLE_TASK idle[cur_cpu]
#define CUR_TASK  curr[cur_cpu]
#define LAST_TASK last[cur_cpu]

spinlock_t task_lock;
cpu_intr_t percpu[NCPU];

#define mycpu (&percpu[cpu_current()])

static void idle_entry(void* arg)
{
    iset(true);
    while(1)
    {
        yield();
    }
}

//---Final Version---begin---
static void insert(task_t* task)
{
    kmt_spin_lock(&task_lock);
    if(task_pool == NULL)
    {
        task_pool = task;
        task->pre = task;
        task->nxt = task;
        kmt_spin_unlock(&task_lock);
        return;
    }
    task_t* head = task_pool;
    task_t* nxt = head->nxt;
    head->nxt = task;
    task->pre = head;
    task->nxt = nxt;
    nxt->pre = task;
    kmt_spin_unlock(&task_lock);
}

static task_t* get_available()
{
    task_t* head = task_pool;
    int pretravel = rand() % 40; 
    for(int i = 0; i < pretravel; i++)
    {
        head = head->nxt;
    }
    task_t* start = head;
    do{
        head = head->nxt;
    }while(head != start && head->status != READY);
    if(head->status != READY) return NULL;
    return head;
}

static Context* kmt_context_save(Event ev, Context* ctx)
{
    TRACE_ENTRY;
    kmt_spin_lock(&task_lock);
    int cur_cpu = cpu_current();
    if(LAST_TASK != NULL && LAST_TASK != CUR_TASK)
    {
        LAST_TASK->status = READY;
    }
    LAST_TASK = CUR_TASK;
    if(CUR_TASK)
        CUR_TASK->ctx = ctx;
    kmt_spin_unlock(&task_lock);
    TRACE_EXIT;
    return NULL;
}

static Context* kmt_schedule(Event ev, Context* ctx)
{
    TRACE_ENTRY;
    int cur_cpu = cpu_current();
    kmt_spin_lock(&task_lock);
    task_t* next = get_available();
    if(next == NULL)
    {
        next = IDLE_TASK;
    }
    CUR_TASK = next;
    CUR_TASK->status = RUNNING;
    kmt_spin_unlock(&task_lock);
    TRACE_EXIT;
    return next->ctx;
}
//---Final Version---end---

static void remove(task_t* task)
{
    task->pre->nxt = task->nxt;
    task->nxt->pre = task->pre;
}

static void enqueue(wl_node_t** wait_list_ptr, task_t* task__)
{
    wl_node_t* task = pmm->alloc(sizeof(wl_node_t));
    task->task = task__;
    task->nxt = NULL;
    task->pre = NULL;
    if(*wait_list_ptr == NULL)
    {
        *wait_list_ptr = task;
        task->pre = task;
        task->nxt = task;
        return;
    }
    wl_node_t* head = *wait_list_ptr;
    wl_node_t* nxt = head->nxt;
    head->nxt = task;
    task->pre = head;
    task->nxt = nxt;
    if (nxt != NULL) { nxt->pre = task; }
}

static task_t* dequeue(wl_node_t** wait_list_ptr)
{
    if(*wait_list_ptr == NULL)
    {
        return NULL;
    }
    wl_node_t* wait_list = *wait_list_ptr;
    if(wait_list->nxt == wait_list)
    {
        task_t* task = wait_list->task;
        pmm->free(wait_list);
        *wait_list_ptr = NULL;
        return task;
    }
    wl_node_t* head = wait_list;
    wl_node_t* task_node = head->nxt;
    if(task_node->nxt == head)
    {
        head->nxt = head;
    } 
    else 
    {
        head->nxt = task_node->nxt;
        task_node->nxt->pre = head;
    }
    task_t* ret = task_node->task;
    pmm->free(task_node);
    return ret;
}

static void kmt_init() 
{
    kmt_spin_init(&task_lock, "task_lock");

    //为每个cpu创建idle task
    for(int i = 0; i < NCPU; i++)
    {
        task_t* task = pmm->alloc(sizeof(task_t));
        task->name = "idle";
        task->status = READY;
        task->stk = pmm->alloc(KMT_STACK_SIZE);
        Area stack = (Area){task->stk, task->stk + KMT_STACK_SIZE}; 
        task->ctx = kcontext(stack, idle_entry, NULL);        
        task->pre = task;
        task->nxt = NULL;
        kmt_spin_init(&(task->runnable_lock), "idle_lock");
        idle[i] = task;
        curr[i] = idle[i];
        last[i] = NULL;
    }
    for(int i = 0; i < NCPU; i++)
    {
        percpu[i].nest = 0;
        percpu[i].intena = 0;
    }

    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg) 
{
    task->name = name;
    task->status = READY;
    task->stk = pmm->alloc(KMT_STACK_SIZE);
    Area stack = (Area){task->stk, task->stk + KMT_STACK_SIZE};
    task->ctx = kcontext(stack, entry, arg);
    kmt_spin_init(&(task->runnable_lock), name);
    insert(task);
    return 0;
}

static void kmt_teardown(task_t *task) 
{
    task->status = TERMINATED;
    kmt_spin_lock(&task_lock);
    remove(task);
    kmt_spin_unlock(&task_lock);
    pmm->free(task->stk);
    pmm->free(task);
}

static void kmt_spin_init(spinlock_t *lk, const char *name) 
{
    lk->name = name;
    lk->status = UNLOCKED;
    lk->cpu = NULL;
}

void push_off()
{
    int i = ienabled();
    iset(0);
    cpu_intr_t *c = mycpu;
    if(c->nest == 0)
    {
        c->intena = i;
    }
    c->nest++;
}

void pop_off()
{
    cpu_intr_t *c = mycpu;
    assert(ienabled() == false);
    assert(c->nest > 0);
    c->nest--;
    if(c->nest == 0 && c->intena)
    {
        iset(1);
    }
}

bool holding(spinlock_t *lk)
{
    return lk->status == LOCKED && lk->cpu == mycpu;
}

static void kmt_spin_lock(spinlock_t *lk)
{
    push_off();
    assert(holding(lk) == false);
    while(atomic_xchg(&lk->status, LOCKED) == LOCKED)
    {
        ;
    }
    lk->cpu = mycpu;
}

static void kmt_spin_unlock(spinlock_t *lk) 
{
    assert(holding(lk) == true);
    lk->cpu = NULL;
    atomic_xchg(&lk->status, UNLOCKED);
    pop_off();
}

static void kmt_sem_init(sem_t *sem, const char *name, int value) 
{
    kmt_spin_init(&sem->lock, name);
    sem->name = name;
    sem->count = value;    
    sem->wait_list = NULL;
}

static void kmt_sem_wait(sem_t *sem) 
{
    TRACE_ENTRY;
    bool succ = false;
    while(!succ)
    {
        kmt_spin_lock(&sem->lock);
        if(sem->count > 0)
        {
            sem->count--;
            succ = true;
        }
        if(!succ)
        {
            int cur_cpu = cpu_current();
            task_t* task = CUR_TASK;
            enqueue(&sem->wait_list, task);
            task->status = BLOCKED;
            kmt_spin_unlock(&sem->lock);
            yield();
            kmt_spin_lock(&sem->lock);
        }
        kmt_spin_unlock(&sem->lock);
    }
}

static void kmt_sem_signal(sem_t *sem) 
{
    TRACE_ENTRY;
    kmt_spin_lock(&sem->lock);
    sem->count++;
    if(sem->count<=0 && sem->wait_list != NULL)
    {
        task_t* task = dequeue(&sem->wait_list);
        task->status = READY;
    }
    kmt_spin_unlock(&sem->lock);
    TRACE_EXIT;
}

MODULE_DEF(kmt) = {
    .init = kmt_init,
    .create = kmt_create,
    .teardown = kmt_teardown,
    .spin_init = kmt_spin_init,
    .spin_lock = kmt_spin_lock,
    .spin_unlock = kmt_spin_unlock,
    .sem_init = kmt_sem_init,
    .sem_wait = kmt_sem_wait,
    .sem_signal = kmt_sem_signal,
};