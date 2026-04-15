/*
 * 04_rt_tasks/main.c
 *
 * TOPIC: Writing Real-Time Tasks in C (SCHED_FIFO / SCHED_RR)
 *
 * What this covers:
 *   - Setting SCHED_FIFO / SCHED_RR via pthread_setschedparam
 *   - mlockall() — why RT tasks must lock memory
 *   - Priority inversion problem + priority inheritance solution
 *   - Multiple RT threads at different priorities
 *   - Periodic RT task with deadline tracking
 *
 * Build:  make
 * Run:    sudo ./rt_tasks     (needs root for RT scheduling)
 * Cross:  make cross
 *
 * RULES for writing RT tasks:
 *   1. Always mlockall() before starting
 *   2. Never malloc() in the RT loop (pre-allocate everything)
 *   3. Never call printf() in the RT loop (use a logging buffer)
 *   4. Never do file I/O in the RT loop
 *   5. Set stack size explicitly
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

/* ----------------------------------------
 * CONFIGURATION
 * ---------------------------------------- */
#define RT_STACK_SIZE       (8 * 1024 * 1024)   /* 8MB stack */
#define TASK_PERIOD_US      1000                /* 1ms period */
#define TASK_ITERATIONS     5000                /* run for 5 seconds */

/* Priority levels (1=lowest, 99=highest for SCHED_FIFO) */
#define PRIO_HIGH_TASK      85
#define PRIO_MED_TASK       70
#define PRIO_LOW_TASK       55

static volatile int g_stop = 0;

/* ----------------------------------------
 * TIMESPEC HELPERS
 * ---------------------------------------- */
static inline void ts_add_us(struct timespec *ts, long us)
{
    ts->tv_nsec += us * 1000L;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec++;
    }
}

static inline long ts_diff_us(struct timespec *a, struct timespec *b)
{
    return (a->tv_sec  - b->tv_sec)  * 1000000L +
           (a->tv_nsec - b->tv_nsec) / 1000L;
}

/* ----------------------------------------
 * RT TASK STRUCTURE
 * ---------------------------------------- */
typedef struct {
    const char *name;
    int         priority;
    int         policy;         /* SCHED_FIFO or SCHED_RR */
    long        period_us;
    long        iterations;

    /* Stats (updated by task, read by main) */
    long        count;
    long        max_latency_us;
    long        missed_deadlines;
    int         done;
} RtTask;

/* ----------------------------------------
 * DEMO 1: Simple periodic RT task
 * This is the skeleton every RT task should follow
 * ---------------------------------------- */
void *periodic_rt_task(void *arg)
{
    RtTask *t = (RtTask *)arg;
    struct timespec next_wakeup, actual;

    printf("  [%s] started — policy=%s priority=%d\n",
           t->name,
           t->policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR",
           t->priority);

    /* Get start time */
    clock_gettime(CLOCK_MONOTONIC, &next_wakeup);

    for (long i = 0; i < t->iterations && !g_stop; i++) {

        /* --- REAL-TIME WORK GOES HERE --- */
        /* In a real application this is where you:
         *   - Read a sensor (your LM75 driver!)
         *   - Control a motor/actuator
         *   - Process a signal
         *   - Communicate over CAN bus
         * For now we just do a short busy wait to simulate work */
        volatile long dummy = 0;
        for (long j = 0; j < 1000; j++) dummy++;
        /* -------------------------------- */

        /* Calculate next activation */
        ts_add_us(&next_wakeup, t->period_us);

        /* Check if we already missed the deadline */
        clock_gettime(CLOCK_MONOTONIC, &actual);
        long latency = ts_diff_us(&actual, &next_wakeup);

        if (latency > 0) {
            t->missed_deadlines++;
            if (latency > t->max_latency_us)
                t->max_latency_us = latency;
        }

        t->count = i;

        /* Sleep until next period */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup, NULL);

        /* Measure actual wakeup latency */
        clock_gettime(CLOCK_MONOTONIC, &actual);
        long wakeup_latency = ts_diff_us(&actual, &next_wakeup);
        if (wakeup_latency > t->max_latency_us)
            t->max_latency_us = wakeup_latency;
    }

    t->done = 1;
    printf("  [%s] done — %ld iterations, max latency: %ldus, missed: %ld\n",
           t->name, t->count, t->max_latency_us, t->missed_deadlines);
    return NULL;
}

/* ----------------------------------------
 * SETUP: create an RT thread properly
 * ---------------------------------------- */
static int create_rt_thread(pthread_t *tid, RtTask *task)
{
    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);

    /* Set scheduling policy */
    pthread_attr_setschedpolicy(&attr, task->policy);

    /* Set priority */
    param.sched_priority = task->priority;
    pthread_attr_setschedparam(&attr, &param);

    /* Must set this or pthread ignores the above */
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    /* Set stack size */
    pthread_attr_setstacksize(&attr, RT_STACK_SIZE);

    int ret = pthread_create(tid, &attr, periodic_rt_task, task);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        fprintf(stderr, "  pthread_create failed: %s\n", strerror(ret));
        fprintf(stderr, "  Hint: run with sudo for RT scheduling\n");
    }
    return ret;
}

/* ----------------------------------------
 * DEMO 2: Priority inversion illustration
 *
 * Classic problem:
 *   High priority task waits for mutex held by low priority task
 *   Medium priority task preempts the low priority task
 *   High priority task is effectively blocked by medium — WRONG!
 *
 * Solution: Priority Inheritance Mutex (PTHREAD_PRIO_INHERIT)
 * ---------------------------------------- */
static pthread_mutex_t g_normal_mutex;
static pthread_mutex_t g_pi_mutex;     /* Priority Inheritance mutex */

typedef struct {
    const char *name;
    int         prio;
    pthread_mutex_t *mutex;
} PiTaskArg;

void *pi_task(void *arg)
{
    PiTaskArg *t = (PiTaskArg *)arg;

    if (strcmp(t->name, "LOW") == 0) {
        /* Low priority task acquires mutex first */
        pthread_mutex_lock(t->mutex);
        printf("    [%s] acquired mutex\n", t->name);

        /* Simulate work holding the lock */
        struct timespec ts = {0, 50000000}; /* 50ms */
        nanosleep(&ts, NULL);

        pthread_mutex_unlock(t->mutex);
        printf("    [%s] released mutex\n", t->name);
    }
    else if (strcmp(t->name, "HIGH") == 0) {
        /* High priority task tries to acquire mutex */
        struct timespec ts = {0, 10000000}; /* wait 10ms first */
        nanosleep(&ts, NULL);

        printf("    [%s] trying to acquire mutex...\n", t->name);
        pthread_mutex_lock(t->mutex);
        printf("    [%s] got mutex!\n", t->name);
        pthread_mutex_unlock(t->mutex);
    }

    return NULL;
}

void demo_priority_inheritance(void)
{
    printf("\n=== DEMO: Priority Inheritance ===\n\n");
    printf("  Without PI mutex: HIGH task blocked by LOW task\n");
    printf("  With PI mutex:    LOW task temporarily inherits HIGH priority\n\n");

    /* Normal mutex (no PI) */
    pthread_mutex_init(&g_normal_mutex, NULL);

    /* PI mutex */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&g_pi_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    printf("  Using PTHREAD_PRIO_INHERIT mutex:\n");

    pthread_t tl, th;
    PiTaskArg low  = {"LOW",  PRIO_LOW_TASK,  &g_pi_mutex};
    PiTaskArg high = {"HIGH", PRIO_HIGH_TASK, &g_pi_mutex};

    pthread_create(&tl, NULL, pi_task, &low);
    pthread_create(&th, NULL, pi_task, &high);

    pthread_join(tl, NULL);
    pthread_join(th, NULL);

    pthread_mutex_destroy(&g_normal_mutex);
    pthread_mutex_destroy(&g_pi_mutex);

    printf("\n  With PI: LOW task runs at HIGH priority while holding mutex\n");
    printf("  This prevents the medium-priority task from blocking HIGH\n\n");
}

/* ----------------------------------------
 * MAIN
 * ---------------------------------------- */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   Real-Time Tasks Demo (BBB)         ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    /* Step 1: Lock all memory — MANDATORY for RT tasks */
    printf("=== Step 1: Lock memory (mlockall) ===\n");
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("  mlockall failed (run as root for best results)");
    } else {
        printf("  ✅ Memory locked — no page faults during RT execution\n");
    }
    printf("\n");

    /* Step 2: Create three RT threads at different priorities */
    printf("=== Step 2: Three periodic RT tasks ===\n\n");

    RtTask high_task = {
        .name       = "HIGH_TASK",
        .priority   = PRIO_HIGH_TASK,
        .policy     = SCHED_FIFO,
        .period_us  = TASK_PERIOD_US,
        .iterations = TASK_ITERATIONS,
    };

    RtTask med_task = {
        .name       = "MED_TASK",
        .priority   = PRIO_MED_TASK,
        .policy     = SCHED_FIFO,
        .period_us  = TASK_PERIOD_US * 2,   /* 2ms period */
        .iterations = TASK_ITERATIONS / 2,
    };

    RtTask low_task = {
        .name       = "LOW_TASK",
        .priority   = PRIO_LOW_TASK,
        .policy     = SCHED_RR,             /* Round-robin */
        .period_us  = TASK_PERIOD_US * 5,   /* 5ms period */
        .iterations = TASK_ITERATIONS / 5,
    };

    pthread_t t_high, t_med, t_low;

    create_rt_thread(&t_high, &high_task);
    create_rt_thread(&t_med,  &med_task);
    create_rt_thread(&t_low,  &low_task);

    pthread_join(t_high, NULL);
    pthread_join(t_med,  NULL);
    pthread_join(t_low,  NULL);

    /* Step 3: Priority inheritance demo */
    demo_priority_inheritance();

    printf("=== Summary ===\n\n");
    printf("  SCHED_FIFO:  runs until it yields/blocks, highest priority wins\n");
    printf("  SCHED_RR:    like FIFO but with time slices between equal-priority tasks\n");
    printf("  mlockall():  prevents page faults that would cause latency spikes\n");
    printf("  PI mutex:    prevents priority inversion in shared resource access\n\n");

    printf("  NEXT: Go to 05_pru/ for hard real-time with BBB's PRU co-processor\n\n");

    return 0;
}
