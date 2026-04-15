/*
 * 01_scheduling/main.c
 *
 * TOPIC: Linux Process Scheduling
 *
 * What this covers:
 *   - Scheduling policies: SCHED_OTHER, SCHED_FIFO, SCHED_RR
 *   - Priority: nice values (-20 to +19) and RT priority (1-99)
 *   - How to read your own scheduler info from /proc
 *   - Comparing CPU time with different priorities
 *
 * Build:  make
 * Run:    ./scheduling
 *
 * Key concepts:
 *   CFS (Completely Fair Scheduler) - default Linux scheduler
 *   SCHED_OTHER  - normal tasks, uses nice values
 *   SCHED_FIFO   - real-time, runs until it yields or blocks
 *   SCHED_RR     - real-time, round-robin with time slices
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

/* ------------------------------------------------
 * HELPER: print current process scheduler info
 * ------------------------------------------------ */
void print_sched_info(const char *label)
{
    int policy = sched_getscheduler(0);  /* 0 = current process */
    struct sched_param param;
    sched_getparam(0, &param);

    int nice_val = getpriority(PRIO_PROCESS, 0);

    const char *policy_name;
    switch (policy) {
        case SCHED_OTHER: policy_name = "SCHED_OTHER (CFS)"; break;
        case SCHED_FIFO:  policy_name = "SCHED_FIFO  (RT)";  break;
        case SCHED_RR:    policy_name = "SCHED_RR    (RT)";  break;
        case SCHED_BATCH: policy_name = "SCHED_BATCH";       break;
        case SCHED_IDLE:  policy_name = "SCHED_IDLE";        break;
        default:          policy_name = "UNKNOWN";           break;
    }

    printf("[%s]\n", label);
    printf("  PID:             %d\n", getpid());
    printf("  Policy:          %s\n", policy_name);
    printf("  RT Priority:     %d (only used in FIFO/RR)\n", param.sched_priority);
    printf("  Nice value:      %d (lower = higher priority)\n", nice_val);
    printf("  Min RT prio:     %d\n", sched_get_priority_min(SCHED_FIFO));
    printf("  Max RT prio:     %d\n", sched_get_priority_max(SCHED_FIFO));
    printf("\n");
}

/* ------------------------------------------------
 * HELPER: read scheduler info from /proc
 * ------------------------------------------------ */
void print_proc_sched(void)
{
    char path[64];
    char line[256];

    snprintf(path, sizeof(path), "/proc/%d/sched", getpid());
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  Cannot open %s (may need root)\n\n", path);
        return;
    }

    printf("[/proc/%d/sched]\n", getpid());
    int lines = 0;
    while (fgets(line, sizeof(line), f) && lines < 10) {
        printf("  %s", line);
        lines++;
    }
    fclose(f);
    printf("\n");
}

/* ------------------------------------------------
 * DEMO 1: show default scheduling policy
 * ------------------------------------------------ */
void demo_default_scheduler(void)
{
    printf("=== DEMO 1: Default Scheduler ===\n\n");
    print_sched_info("Current process (default)");
    print_proc_sched();
}

/* ------------------------------------------------
 * DEMO 2: change nice value
 * Nice values: -20 (highest priority) to +19 (lowest)
 * Only root can set negative nice values
 * ------------------------------------------------ */
void demo_nice_values(void)
{
    printf("=== DEMO 2: Nice Values ===\n\n");

    printf("Nice value = 0 (default):\n");
    print_sched_info("Before nice change");

    /* Try to increase priority (needs root for negative values) */
    if (nice(-5) == -1 && errno != 0) {
        printf("  Note: setting nice -5 requires root\n");
        nice(5);  /* lower priority instead */
    }
    print_sched_info("After nice change");

    /* Reset */
    nice(0);
}

/* ------------------------------------------------
 * DEMO 3: CPU bound work — measure time taken
 * Shows how scheduler affects task execution
 * ------------------------------------------------ */
void demo_cpu_work(void)
{
    printf("=== DEMO 3: CPU Bound Work ===\n\n");

    struct timespec start, end;
    volatile long count = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Busy loop — 100 million iterations */
    for (long i = 0; i < 100000000L; i++)
        count++;

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_ms = (end.tv_sec  - start.tv_sec)  * 1000 +
                      (end.tv_nsec - start.tv_nsec) / 1000000;

    printf("  100M iterations took: %ld ms\n", elapsed_ms);
    printf("  Count: %ld\n\n", count);
    printf("  Try running with: nice -n 19 ./scheduling  (low priority)\n");
    printf("  vs:               nice -n -20 ./scheduling (high priority, needs root)\n\n");
}

/* ------------------------------------------------
 * DEMO 4: two threads — same vs different priority
 * ------------------------------------------------ */

typedef struct {
    int    id;
    int    nice_val;
    long   count;
    int    done;
} ThreadArg;

void *worker_thread(void *arg)
{
    ThreadArg *t = (ThreadArg *)arg;

    /* Set nice value for this thread */
    setpriority(PRIO_PROCESS, syscall(SYS_gettid), t->nice_val);

    printf("  Thread %d started (nice=%d, tid=%ld)\n",
           t->id, t->nice_val, syscall(SYS_gettid));

    /* Count for 2 seconds */
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                       (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= 2000) break;
        t->count++;
    }

    t->done = 1;
    printf("  Thread %d done: counted %ld iterations\n", t->id, t->count);
    return NULL;
}

void demo_thread_priority(void)
{
    printf("=== DEMO 4: Two Threads, Different Nice Values ===\n\n");
    printf("  Running 2 threads for 2 seconds:\n");
    printf("  Thread 1: nice =  0 (normal)\n");
    printf("  Thread 2: nice = 10 (lower priority)\n\n");

    pthread_t t1, t2;
    ThreadArg a1 = {1,  0, 0, 0};
    ThreadArg a2 = {2, 10, 0, 0};

    pthread_create(&t1, NULL, worker_thread, &a1);
    pthread_create(&t2, NULL, worker_thread, &a2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("\n  Result: Thread 1 got ~%ldx more CPU than Thread 2\n\n",
           a2.count > 0 ? a1.count / a2.count : 0);

    printf("  WHY: CFS gives more CPU time to higher priority tasks\n");
    printf("  This is the fundamental job of the scheduler.\n\n");
}

/* ------------------------------------------------
 * DEMO 5: Check RT scheduling priority range
 * ------------------------------------------------ */
void demo_rt_info(void)
{
    printf("=== DEMO 5: RT Scheduling Info ===\n\n");

    printf("  SCHED_FIFO  priority range: %d - %d\n",
           sched_get_priority_min(SCHED_FIFO),
           sched_get_priority_max(SCHED_FIFO));

    printf("  SCHED_RR    priority range: %d - %d\n",
           sched_get_priority_min(SCHED_RR),
           sched_get_priority_max(SCHED_RR));

    printf("  SCHED_OTHER priority range: %d - %d\n\n",
           sched_get_priority_min(SCHED_OTHER),
           sched_get_priority_max(SCHED_OTHER));

    printf("  NOTE: To actually use SCHED_FIFO/RR you need:\n");
    printf("    1. Root privileges (or CAP_SYS_NICE)\n");
    printf("    2. A PREEMPT_RT kernel (for true real-time)\n");
    printf("    3. mlockall() to prevent page faults\n\n");
    printf("  --> That's what 04_rt_tasks covers!\n\n");
}

/* ------------------------------------------------
 * MAIN
 * ------------------------------------------------ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   Linux Scheduling Demo              ║\n");
    printf("║   BeagleBone Black / ARM             ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    demo_default_scheduler();
    demo_nice_values();
    demo_cpu_work();
    demo_thread_priority();
    demo_rt_info();

    printf("=== NEXT STEP ===\n");
    printf("  Go to 02_preempt_rt/ to patch the kernel for real RT\n\n");

    return 0;
}
