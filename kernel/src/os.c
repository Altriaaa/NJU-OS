#include <os.h>
#include <devices.h>

handler_list_t *header = NULL;

// static void tty_reader(void *arg) {
//     device_t *tty = dev->lookup(arg);
//     char cmd[128], resp[128], ps[16];
//     snprintf(ps, 16, "(%s) $ ", arg);
//     while (1) {
//         tty->ops->write(tty, 0, ps, strlen(ps));
//         int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
//         cmd[nread] = '\0';
//         sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
//         tty->ops->write(tty, 0, resp, strlen(resp));
//     }
// }

static void os_init() 
{
    pmm->init();
    kmt->init();
    // dev->init();
    // kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty1");
    // kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty2");
}

static void os_run() {
    iset(true);
    while(1)
    {
        ;
    }
}

static Context* os_trap(Event ev, Context* context) 
{
    TRACE_ENTRY;
    Context* next = NULL;
    handler_list_t* iter = header;
    for(; iter != NULL; iter = iter->next)
    {
        if(iter->event == EVENT_NULL || iter->event == ev.event)
        {
            Context* r = iter->handler(ev, context);
            panic_on(r && next, "return to multiple handlers");
            if(r) next = r;
        }
    }
    panic_on(!next, "return to NULL context");
    TRACE_EXIT;
    return next;
}

static void os_on_irq(int seq, int event, handler_t handler) 
{
    handler_list_t* iter = header;
    handler_list_t* pre = NULL;
    while(iter != NULL && iter->seq < seq)
    {
        pre = iter;
        iter = iter->next;
    }
    handler_list_t* new_handler = (handler_list_t*)pmm->alloc(sizeof(handler_list_t));
    new_handler->seq = seq;
    new_handler->event = event;
    new_handler->handler = handler;
    new_handler->next = iter;
    if(pre == NULL)
    {
        header = new_handler;
    }
    else
    {
        pre->next = new_handler;
    }
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
    .trap = os_trap,
    .on_irq = os_on_irq,
};
