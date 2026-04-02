#ifndef SCHED_UTILS_H
#define SCHED_UTILS_H

typedef enum {
    PRIO_HIGH   = 0,
    PRIO_NORMAL = 1,
    PRIO_LOW    = 2,
    PRIO_IDLE   = 3,

    PRIORITY_LEVELS = 4,
    PRIORITY_BOOST  = 100
} task_prio_t;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE,
    TASK_FINISHED,
    TASK_BLOCKED
} task_state_t;

typedef enum {
    REASON_KEYBOARD
} task_reason_t;

#endif
