#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

typedef void (*task_fn)(void *);

struct scheduler_task_info {
    int id;
    int used;
    int dead;
};

int scheduler_init(void); 
int task_create(task_fn fn, void *arg);
void scheduler_run(void);
void scheduler_yield(void);
int scheduler_get_current(void);
void scheduler_mark_dead(int id);
int scheduler_get_tasks(struct scheduler_task_info *out, int max);
void scheduler_lock(void);
void scheduler_unlock(void);

#endif
