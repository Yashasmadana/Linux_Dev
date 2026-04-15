/*
 * 03_latency/main.c
 *
 * TOPIC: Measuring Scheduling Latency on BBB
 *
 * What this does:
 *   - Wakes up every TARGET_INTERVAL_US microseconds
 *   - Measures actual wakeup time vs expected
 *   - Records min, max, average latency
 *   - Shows a histogram of latency distribution
 *
 * This is essentially a simplified version of 'cyclictest'
 * (the standard RT latency measurement tool)
 *
 * Build:   make
 * Run:     sudo ./latency              (standard kernel)
 *          sudo ./latency 10000        (10000 iterations)
 * Cross:   make cross
 *
 * Run on BBB BEFORE and AFTER applying PREEMPT_RT patch
 * to see the difference in worst-case latency.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <limits.h>
#include <errno.h>

/* ----------------------------------------
 * CONFIGURATION
 * ---------------------------------------- */
#define TARGET_INTERVAL_US  1000        /* Wake up every 1ms */
#define DEFAULT_ITERATIONS  10000       /* Number of measurements */
#define HISTOGRAM_SLOTS     100         /* Latency histogram buckets */
#define HISTOGRAM_MAX_US    1000        /* Max latency shown in histogram */

/* ----------------------------------------
 * DATA STRUCTURES
 * ---------------------------------------- */
typedef struct {
    long min_us;
    long max_us;
    long sum_us;
    long count;
    long overruns;                      /* latency > 2x target */
    long histogram[HISTOGRAM_SLOTS];   /* us distribution */
} LatencyStats;

static volatile int g_stop = 0;

/* ----------------------------------------
 * SIGNAL HANDLER
 * ---------------------------------------- */
static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ----------------------------------------
 * TIMESPEC HELPERS
 * ---------------------------------------- */
static inline long timespec_diff_us(struct timespec *a, struct timespec *b)
{
    return (a->tv_sec  - b->tv_sec)  * 1000000L +
           (a->tv_nsec - b->tv_nsec) / 1000L;
}

static inline void timespec_add_us(struct timespec *ts, long us)
{
    ts->tv_nsec += us * 1000L;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec++;
    }
}

/* ----------------------------------------
 * SETUP: lock memory + set RT priority
 * ---------------------------------------- */
static void setup_rt(void)
{
    /* Lock all memory to prevent page faults during measurements */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("mlockall failed (try running as root)");
    }

    /* Set SCHED_FIFO priority 80 */
    struct sched_param param = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &param) < 0) {
        perror("sched_setscheduler (try running as root)");
        printf("  Running without RT priority — results will be less accurate\n\n");
    } else {
        printf("  RT priority set: SCHED_FIFO prio=80\n");
        printf("  Memory locked: mlockall()\n\n");
    }
}

/* ----------------------------------------
 * CORE: measure latency loop
 * ---------------------------------------- */
static void measure_latency(LatencyStats *stats, long iterations)
{
    struct timespec expected, actual;

    /* Start from now */
    clock_gettime(CLOCK_MONOTONIC, &expected);

    for (long i = 0; i < iterations && !g_stop; i++) {

        /* Calculate next wakeup time */
        timespec_add_us(&expected, TARGET_INTERVAL_US);

        /* Sleep until expected time */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &expected, NULL);

        /* Measure actual wakeup time */
        clock_gettime(CLOCK_MONOTONIC, &actual);

        /* Calculate latency (how late we woke up) */
        long latency_us = timespec_diff_us(&actual, &expected);
        if (latency_us < 0) latency_us = 0;

        /* Update stats */
        if (latency_us < stats->min_us) stats->min_us = latency_us;
        if (latency_us > stats->max_us) stats->max_us = latency_us;
        stats->sum_us += latency_us;
        stats->count++;

        if (latency_us > TARGET_INTERVAL_US * 2)
            stats->overruns++;

        /* Fill histogram */
        long slot = latency_us * HISTOGRAM_SLOTS / HISTOGRAM_MAX_US;
        if (slot >= HISTOGRAM_SLOTS) slot = HISTOGRAM_SLOTS - 1;
        stats->histogram[slot]++;

        /* Print progress every 1000 iterations */
        if (i % 1000 == 0) {
            printf("\r  Progress: %ld/%ld  current: %ldus  max: %ldus    ",
                   i, iterations, latency_us, stats->max_us);
            fflush(stdout);
        }
    }
    printf("\n");
}

/* ----------------------------------------
 * PRINT: results + histogram
 * ---------------------------------------- */
static void print_results(LatencyStats *stats)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║         LATENCY RESULTS              ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    printf("  Target interval : %d us\n",   TARGET_INTERVAL_US);
    printf("  Iterations      : %ld\n",     stats->count);
    printf("  Min latency     : %ld us\n",  stats->min_us);
    printf("  Avg latency     : %ld us\n",  stats->count ? stats->sum_us / stats->count : 0);
    printf("  Max latency     : %ld us\n",  stats->max_us);
    printf("  Overruns        : %ld\n\n",   stats->overruns);

    /* Interpretation */
    printf("  Interpretation:\n");
    if (stats->max_us < 100)
        printf("  ✅ Excellent — PREEMPT_RT kernel active\n");
    else if (stats->max_us < 1000)
        printf("  ⚠️  Good — standard kernel, moderate load\n");
    else
        printf("  ❌ High latency — standard kernel under load\n");

    printf("\n");

    /* Histogram */
    printf("  Latency Histogram (0 - %d us):\n\n", HISTOGRAM_MAX_US);

    long max_count = 0;
    for (int i = 0; i < HISTOGRAM_SLOTS; i++)
        if (stats->histogram[i] > max_count)
            max_count = stats->histogram[i];

    for (int i = 0; i < HISTOGRAM_SLOTS; i += 2) {
        if (stats->histogram[i] == 0 && stats->histogram[i+1] == 0)
            continue;

        int us = i * HISTOGRAM_MAX_US / HISTOGRAM_SLOTS;
        printf("  %4d us | ", us);

        int bar = max_count > 0
                  ? (int)((stats->histogram[i] + stats->histogram[i+1]) * 40 / max_count)
                  : 0;
        for (int b = 0; b < bar; b++) printf("#");
        printf(" %ld\n", stats->histogram[i] + stats->histogram[i+1]);
    }

    printf("\n");
    printf("  Compare these results:\n");
    printf("  BEFORE PREEMPT_RT: max latency typically > 1000us\n");
    printf("  AFTER  PREEMPT_RT: max latency typically <  100us\n\n");
}

/* ----------------------------------------
 * MAIN
 * ---------------------------------------- */
int main(int argc, char *argv[])
{
    long iterations = DEFAULT_ITERATIONS;

    if (argc > 1)
        iterations = atol(argv[1]);

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   BBB Latency Measurement Tool       ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    printf("  Measuring %ld wakeups at %d us intervals\n",
           iterations, TARGET_INTERVAL_US);
    printf("  Press Ctrl+C to stop early\n\n");

    /* Setup RT environment */
    setup_rt();

    /* Init stats */
    LatencyStats stats = {
        .min_us = LONG_MAX,
        .max_us = 0,
    };

    /* Run measurement */
    measure_latency(&stats, iterations);

    /* Print results */
    print_results(&stats);

    printf("  NEXT: Go to 04_rt_tasks/ to write your own RT task\n\n");

    return 0;
}
