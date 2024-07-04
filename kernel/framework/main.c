// DO NOT MODIFY: Will be reverted by the Online Judge.

#include <kernel.h>
#include <klib.h>
#include <pthread.h>

enum ops{ OP_ALLOC = 1, OP_FREE = 2 };

void* addrs[1000];

struct malloc_op
{
    enum ops type;
    union
    {
        size_t sz;
        void* addr;
    };
};

struct malloc_op rand_op()
{
    struct malloc_op op;
    op.type = rand() % 2 + 1;
    if(op.type == OP_ALLOC)
    {
        op.sz = rand() % (8 << 10) + 1;
    }
    else
    {
        op.addr = 0;
        for(int i = 0; i < 1000; i++)
        {
            if(addrs[i] != 0)
            {
                op.addr = addrs[i];
                addrs[i] = 0;
                break;
            }
        }
    }
    return op;
}

void* stress_test_thread()
{
    for(int i = 0; i < 500; i++)
    {
        struct malloc_op op = rand_op();
        if(op.type == OP_ALLOC)
        {
            addrs[i] = pmm->alloc(op.sz);
            if(addrs[i] == 0) printf("alloc failed\n");
        }
        else
        {
            pmm->free((void*)op.addr);
        }
    }
    return NULL;
}

void stress_test()
{
    pthread_t threads[2];

    printf("start test\n");

    for(int i = 0; i < 2; i++)
    {
        pthread_create(&threads[i], NULL, stress_test_thread, NULL);
    }

    for(int i = 0; i < 2; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

int main() {
    os->init();
    mpe_init(stress_test);
    return 1;
}
