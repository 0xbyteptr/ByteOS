#include "multitasking/scheduler.h"
#include <stdint.h>
#include <stddef.h>
#include "mem/alloc.h"
#include "serial/serial.h"
#include "kernel/kernel.h"

#define MAX_TASKS 16
#define STACK_SIZE (16 * 1024)

typedef void (*task_fn)(void *);

struct task {
    int used;
    int dead;
    uint64_t *sp;
    void *stack;
    void *kernel_stack; /* per-task kernel stack for syscall/interrupt handling */
};

static struct task tasks[MAX_TASKS];
static int current = -1;

struct scheduler_task_info {
    int id;
    int used;
    int dead;
};

extern void scheduler_switch(uint64_t **old_sp, uint64_t *new_sp);

/* ======================================================= */
/* Task trampoline – ABI CORRECT                            */
/* ======================================================= */
__attribute__((noreturn))
static void task_trampoline(void)
{
    serial_puts("task_trampoline: entered\n");
    void (*fn)(void *) = NULL;
    void *arg = NULL;
    uint64_t cursp = 0;
    asm volatile ("mov %%rsp, %0" : "=r" (cursp));

    /* Read fn/arg from the stack where task_create placed them */
    uint64_t mem0 = *(uint64_t *)(uintptr_t)cursp;
    uint64_t mem1 = *(uint64_t *)(uintptr_t)(cursp + 8);
    fn = (void (*)(void *))(uintptr_t)mem0;
    arg = (void *)(uintptr_t)mem1;

    /* Advance stack past fn/arg */
    asm volatile ("addq $16, %%rsp" : : : "memory");

    if (fn) {
        serial_puts("task_trampoline: calling fn\n");
        fn(arg);
        serial_puts("task_trampoline: fn returned\n");
    } else {
        serial_puts("task_trampoline: fn is NULL, halting\n");
    }

    tasks[current].dead = 1;
    serial_puts("task_trampoline: marking task as dead\n");
    scheduler_yield(); // Oddaj kontrolę z powrotem do schedulera
    for(;;) asm volatile("hlt"); // Powinno nigdy nie zostać osiągnięte
}

/* ======================================================= */

int scheduler_init(void)
{
    serial_puts("scheduler: init start\n");
    for (int i = 0; i < MAX_TASKS; ++i) {
        tasks[i].used = 0;
        tasks[i].dead = 0;
        tasks[i].sp = NULL;
        // tasks[i].stack = NULL;
        tasks[i].kernel_stack = NULL;
    }
    current = -1;
    serial_puts("scheduler: init done\n");
    return 0;  // Zwracamy 0, aby wskazać sukces
}


/* helpers */
int scheduler_get_current(void) { return current; }
void scheduler_mark_dead(int id) { if (id >= 0 && id < MAX_TASKS) tasks[id].dead = 1; }

/* ======================================================= */

int task_create(task_fn fn, void *arg)
{
    serial_puts("task_create: entry\n");
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (!tasks[i].used) {
            serial_puts("task_create: found slot ");
            serial_putdec((uint64_t)i);
            serial_puts("\n");

            void *stack = kmalloc(STACK_SIZE);
            void *kernel_stack = kmalloc(STACK_SIZE);
            serial_puts("task_create: kmalloc -> ");
            serial_putdec((uint64_t)(uintptr_t)stack);
            serial_puts("\n");
            serial_puts("task_create: kernel stack -> ");
            serial_putdec((uint64_t)(uintptr_t)kernel_stack);
            serial_puts("\n");

            if (!stack || !kernel_stack) {
                serial_puts("task_create: kmalloc failed\n");
                if (stack) kfree(stack);
                if (kernel_stack) kfree(kernel_stack);
                return -1;
            }

            uint8_t *stack_start = (uint8_t *)stack;
            uint8_t *stack_end = stack_start + STACK_SIZE;

            uint64_t *sp = (uint64_t *)stack_end;
            serial_puts("task_create: sp top -> ");
            serial_putdec((uint64_t)(uintptr_t)sp);
            serial_puts("\n");

            /* Align stack to 16 bytes */
            sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);
            serial_puts("task_create: sp aligned -> ");
            serial_putdec((uint64_t)(uintptr_t)sp);
            serial_puts("\n");

            /* Reserve 9 qwords for initial registers/args */
            sp -= 9;

            /* Ensure stack does not underflow the allocated region */
            if ((uint8_t *)sp < stack_start) {
                serial_puts("task_create: stack pointer out of range!\n");
                serial_puts("  stack_start=");
                serial_puthex64((uint64_t)(uintptr_t)stack_start);
                serial_puts("  stack_end=");
                serial_puthex64((uint64_t)(uintptr_t)stack_end);
                serial_puts("  sp=");
                serial_puthex64((uint64_t)(uintptr_t)sp);
                kfree(stack);
                kfree(kernel_stack);
                return -1;
            }

            sp[0] = 0; /* rbp */
            sp[1] = 0; /* rbx */
            sp[2] = 0; /* r12 */
            sp[3] = 0; /* r13 */
            sp[4] = 0; /* r14 */
            sp[5] = 0; /* r15 */
            sp[6] = (uint64_t)task_trampoline; /* ret -> trampoline */
            sp[7] = (uint64_t)fn; /* trampoline will see this as fn */
            sp[8] = (uint64_t)arg; /* trampoline will see this as arg */
            serial_puts("task_create: prepared stack frame (sp=");
            serial_puthex64((uint64_t)(uintptr_t)sp);
            serial_puts(")\n");

            tasks[i].used = 1;
            tasks[i].dead = 0;
            tasks[i].sp = sp;
            tasks[i].stack = stack;
            tasks[i].kernel_stack = kernel_stack;

            serial_puts("task_create: returning ");
            serial_putdec((uint64_t)i);
            serial_puts("\n");
            return i;
        }
    }
    serial_puts("task_create: no slots\n");
    return -1;
}

/* ======================================================= */

static int pick_next(void)
{
    if (current < 0) {
        for (int i = 0; i < MAX_TASKS; ++i) {
            if (tasks[i].used && !tasks[i].dead)
                return i;
        }
    } else {
        for (int i = 1; i <= MAX_TASKS; ++i) {
            int idx = (current + i) % MAX_TASKS;
            if (tasks[idx].used && !tasks[idx].dead)
                return idx;
        }
    }
    return -1;
}


static int sched_lock = 0;
void scheduler_lock(void)   { sched_lock++; }
void scheduler_unlock(void) { if (sched_lock > 0) sched_lock--; }

void scheduler_yield(void)
{
    serial_puts("scheduler_yield: entry\n");
    if (sched_lock > 0) {
        serial_puts("scheduler_yield: locked\n");
        return;
    }

    int next = pick_next();
    if (next < 0) {
        serial_puts("scheduler_yield: no next task\n");
        return;
    }

    if (next == current) {
        serial_puts("scheduler_yield: no switch needed\n");
        return;
    }

    int prev = current;
    current = next;

    serial_puts("scheduler_yield: switching from ");
    serial_putdec((uint64_t)prev);
    serial_puts(" to ");
    serial_putdec((uint64_t)current);
    serial_puts("\n");

    if (prev >= 0) {
        scheduler_switch(&tasks[prev].sp, tasks[next].sp);
    } else {
        uint64_t *dummy = NULL;
        scheduler_switch(&dummy, tasks[next].sp);
    }
    serial_puts("scheduler_yield: switched\n");
}

int scheduler_get_tasks(struct scheduler_task_info *out, int max)
{
    if (!out || max <= 0)
        return 0;

    int count = 0;

    for (int i = 0; i < MAX_TASKS && count < max; ++i) {
        if (tasks[i].used) {
            out[count].id   = i;
            out[count].used = tasks[i].used;
            out[count].dead = tasks[i].dead;
            count++;
        }
    }

    return count;
}

/* ======================================================= */

void scheduler_run(void)
{
    serial_puts("scheduler: run start\n");

    while (1) {
        current = pick_next();
        if (current < 0) {
            serial_puts("scheduler: no tasks to run\n");
            break;
        }

        serial_puts("scheduler: switching to task ");
        serial_putdec((uint64_t)current);
        serial_puts("\n");

        uint64_t *dummy = NULL;
        scheduler_switch(&dummy, tasks[current].sp);
    }

    serial_puts("scheduler: no more tasks, halting\n");
    for (;;) asm volatile("hlt");
}
