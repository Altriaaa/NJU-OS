#include <common.h>
#include "spinlock.h"

#define NCPU 8
#define PAGE_SIZE (64 << 10)
struct cpu cpus[NCPU];

//page中的内存单元的结构体,每个内存单元的大小是2的幂次方
typedef struct pagenode
{
    struct pagenode* next;
    int size;
} pagenode;

//page, 每页4KB
typedef struct page
{
    struct page* next;     //指向物理内存中的下一个page
    struct page* cpu_next; //指向在CPU的链表中的下一个page
    int size;   //page中存放的内存单元的大小,全都按照2的幂次方来分配
    pagenode* free_list;    //page中空闲内存单元的链表
} page;

//每个CPU维护8条page链表
typedef struct pagelist
{
    page* pages_32B;
    page* pages_64B;
    page* pages_128B;
    page* pages_256B;
    page* pages_512B;
    page* pages_1024B;
    page* pages_2048B;
    page* pages_4096B;
    spinlock_t lock;
} pagelist;

pagelist cpu_pagelist[NCPU]; //最多8个CPU
spinlock_t global_lock;
page* global_page_list = NULL;

void init_page(page* p, int size)
{
    p->size = size;
    p->cpu_next = NULL;
    //初始化空闲链表
    p->free_list = (void*)p + size;
    p->free_list->size = size;
    pagenode* cur = p->free_list;
    int unit_num = PAGE_SIZE / size;
    for(int i = 1; i < unit_num - 1; i++)
    {
        cur->next = (void*)cur + size;
        cur = cur->next;
        cur->size = size;
    }
    cur->next = NULL;
    cur->size = size;
}

void* alloc_in_page(page* p)
{
    if(p->free_list == NULL)
    {
        return NULL;
    }
    void* ret = p->free_list;
    p->free_list = p->free_list->next;
    // printf("in-page alloc success, get memory starts at %p, ends at %p\n", ret, (void*)((uintptr_t)ret + p->size));
    return ret;
}

void free_in_page(page* p, void* addr)
{
    pagenode* node = (pagenode*)addr;
    node->next = p->free_list;
    p->free_list = node;
    // printf("in-page free success, free memory starts at %p, ends at %p\n", addr, (void*)((uintptr_t)addr + p->size));
}

page* alloc_page()
{
    if(global_page_list == NULL)
    {
        return NULL;
    }
    page* p = global_page_list;
    global_page_list = global_page_list->next;
    return p;
}

// 分配大于4K的内存
static void* alloc_large(size_t size)
{
    int i = 1;
    while(i < size)
    {
        i = i << 1;
    }
    size = i;
    int page_num = size / PAGE_SIZE; //需要的页数
    if(page_num * PAGE_SIZE < size) page_num++;
    if(page_num > 1)  page_num *= 2; // 添加额外的页面以容纳对齐
    page* p = global_page_list;
    page* pre = NULL;
    void* result = NULL;
    while(p != NULL)
    {
        int cnt = 1;
        page* cur = p;
        while(cur->next != NULL && cnt < page_num)
        {
            cur = cur->next;
            cnt++;
        }
        if(cnt == page_num)
        {
            if(pre == NULL) global_page_list = cur->next;
            else pre->next = cur->next;
            cur->next = NULL;
            // 对齐操作
            uintptr_t addr = (uintptr_t)p;
            uintptr_t aligned_addr = (addr + size - 1) & ~(size - 1);
            result = (void*)aligned_addr;
            break;
        }
        pre = p;
        p = p->next;
    }
    return result; 
}

static void *kalloc(size_t size)
{
    if(size <= 32) size = 32;
    int i = 1;
    while(i < size)
    {
        i = i << 1;
    }
    size = i;

    if(size > (16 << 20)) return NULL;

    if(size > (4 << 10))
    {
        spin_lock(&global_lock);
        void* addr = alloc_large(size);
        spin_unlock(&global_lock);
        // assert(addr == NULL || (uintptr_t)addr % size == 0);
        return addr;
    }

    int cpuid = cpu_current();
    page* p = NULL;
    switch(size)
    {
        case 32:
            p = cpu_pagelist[cpuid].pages_32B;
            break;
        case 64:
            p = cpu_pagelist[cpuid].pages_64B;
            break;
        case 128:
            p = cpu_pagelist[cpuid].pages_128B;
            break;
        case 256:
            p = cpu_pagelist[cpuid].pages_256B;
            break;
        case 512:
            p = cpu_pagelist[cpuid].pages_512B;
            break;
        case 1024:
            p = cpu_pagelist[cpuid].pages_1024B;
            break;
        case 2048:
            p = cpu_pagelist[cpuid].pages_2048B;
            break;
        case 4096:
            p = cpu_pagelist[cpuid].pages_4096B;
            break;
    }
    if(p == NULL)
    {
        spin_lock(&global_lock);
        p = alloc_page();
        spin_unlock(&global_lock);
        if(p == NULL)
        {
            return NULL;
        }
        init_page(p, size);
        // spin_lock(&cpu_pagelist[cpuid].lock);
        switch(size)
        {
            case 32:
                cpu_pagelist[cpuid].pages_32B = p;
                break;
            case 64:
                cpu_pagelist[cpuid].pages_64B = p;
                break;
            case 128:
                cpu_pagelist[cpuid].pages_128B = p;
                break;
            case 256:
                cpu_pagelist[cpuid].pages_256B = p;
                break;
            case 512:
                cpu_pagelist[cpuid].pages_512B = p;
                break;
            case 1024:
                cpu_pagelist[cpuid].pages_1024B = p;
                break;
            case 2048:
                cpu_pagelist[cpuid].pages_2048B = p;
                break;
            case 4096:
                cpu_pagelist[cpuid].pages_4096B = p;
                break;
        }
        // spin_unlock(&cpu_pagelist[cpuid].lock);
    }
    void* addr = NULL;
    page* cur = p;
    // spin_lock(&cpu_pagelist[cpuid].lock);
    for(; addr == NULL && cur != NULL; cur = cur->cpu_next)
    {
        addr = alloc_in_page(p);
    }
    // spin_unlock(&cpu_pagelist[cpuid].lock);
    if(addr == NULL)
    {
        spin_lock(&global_lock);
        page* new_page = alloc_page();
        spin_unlock(&global_lock);
        // printf("alloc new page\n");
        if(new_page == NULL)
        {
            return NULL;
        }
        init_page(new_page, size);
        // spin_lock(&cpu_pagelist[cpuid].lock);
        new_page->cpu_next = p;
        switch(size)
        {
            case 32:
                cpu_pagelist[cpuid].pages_32B = new_page;
                break;
            case 64:
                cpu_pagelist[cpuid].pages_64B = new_page;
                break;
            case 128:
                cpu_pagelist[cpuid].pages_128B = new_page;
                break;
            case 256:
                cpu_pagelist[cpuid].pages_256B = new_page;
                break;
            case 512:
                cpu_pagelist[cpuid].pages_512B = new_page;
                break;
            case 1024:
                cpu_pagelist[cpuid].pages_1024B = new_page;
                break;
            case 2048:
                cpu_pagelist[cpuid].pages_2048B = new_page;
                break;
            case 4096:
                cpu_pagelist[cpuid].pages_4096B = new_page;
                break;
        }
        addr = alloc_in_page(new_page);
        // spin_unlock(&cpu_pagelist[cpuid].lock);
    }
    // assert(addr == NULL || (uintptr_t)addr % size == 0);
    return addr;
}

static void kfree(void *ptr) {
    return ;
    // if(cpu_count() >= 2) return;
    uintptr_t addr = (uintptr_t)ptr;
    // for(int i = 0; i < cpu_count(); i++)
    // {
        int i = cpu_current();
        // spin_lock(&cpu_pagelist[i].lock);
        page* p = cpu_pagelist[i].pages_32B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_64B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_128B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_256B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_512B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_1024B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_2048B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        p = cpu_pagelist[i].pages_4096B;
        for(; p != NULL; p = p->cpu_next)
        {
            if((uintptr_t)p <= addr && addr < (uintptr_t)p + PAGE_SIZE)
            {
                free_in_page(p, ptr);
                // spin_unlock(&cpu_pagelist[i].lock);
                return;
            }
        }
        // spin_unlock(&cpu_pagelist[i].lock);
    // }
    // printf("free failed\n");
}

static void pmm_init() {
    uintptr_t pmsize = (
        (uintptr_t)heap.end
        - (uintptr_t)heap.start
    );

    // printf(
    //     "Got %d MiB heap: [%p, %p)\n",
    //     pmsize >> 20, heap.start, heap.end
    // );

    //初始化全局锁
    global_lock = spin_init("pmm_global");

    //初始化每个CPU的page链表
    for (int i = 0; i < cpu_count(); i++) 
    {
        cpu_pagelist[i].pages_32B = NULL;
        cpu_pagelist[i].pages_64B = NULL;
        cpu_pagelist[i].pages_128B = NULL;
        cpu_pagelist[i].pages_256B = NULL;
        cpu_pagelist[i].pages_512B = NULL;
        cpu_pagelist[i].pages_1024B = NULL;
        cpu_pagelist[i].pages_2048B = NULL;
        cpu_pagelist[i].pages_4096B = NULL;
    }

    //初始化每个CPU的锁
    for (int i = 0; i < cpu_count(); i++) 
    {
        cpu_pagelist[i].lock = spin_init("pmm_cpu");
    }

    //初始化全局空闲链表
    int page_num = pmsize / PAGE_SIZE;
    // printf("get page_num: %d\n", page_num);
    page* tail = global_page_list;
    if (tail == NULL) {
        global_page_list = (page*)((void*)heap.start);
        tail = global_page_list;
    } else {
        while (tail->next != NULL) {
            tail = tail->next;
        }
    }
    for(int i = 1; i < page_num; i++) 
    {
        page* p = (page*)((void*)heap.start+i*PAGE_SIZE);
        p->next = NULL;
        tail->next = p;
        tail = p;
    }
    // printf("pmm initialized\n");
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};

