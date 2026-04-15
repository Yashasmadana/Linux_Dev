/*
 * 05_pru/blink.c
 *
 * TOPIC: BBB PRU (Programmable Real-time Unit)
 *
 * The BBB has TWO PRU cores built into the AM335x SoC:
 *   - Each runs at 200MHz
 *   - Each instruction takes exactly 5ns
 *   - Completely independent from Linux kernel
 *   - Direct access to GPIO without kernel overhead
 *   - ZERO jitter — perfect for hard real-time tasks
 *
 * This is the HOST SIDE (Linux userspace) code.
 * It communicates with the PRU via shared memory (PRUSS).
 *
 * PRU firmware (blink.asm) runs ON the PRU core.
 * This file runs ON the ARM (Linux side).
 *
 * Build:   make
 * Run:     sudo ./blink
 *
 * What this does:
 *   - Loads PRU firmware
 *   - Sends blink frequency via shared memory
 *   - PRU blinks P8_11 (GPIO) at exact frequency
 *   - Linux cannot achieve this precision — PRU can
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <signal.h>

/* ----------------------------------------
 * PRU MEMORY MAP (AM335x)
 * ----------------------------------------
 *
 * PRUSS (PRU Subsystem) memory regions:
 *
 * 0x4A300000 — PRU0 instruction RAM (8KB)
 * 0x4A302000 — PRU1 instruction RAM (8KB)
 * 0x4A310000 — PRU0 data RAM       (8KB)
 * 0x4A312000 — PRU1 data RAM       (8KB)
 * 0x4A326000 — Shared RAM          (12KB) ← we use this for communication
 *
 * Linux exposes this via /dev/mem or remoteproc
 * ---------------------------------------- */

#define PRUSS_BASE          0x4A300000
#define PRUSS_SIZE          0x80000
#define PRU_SHARED_RAM_OFF  0x26000     /* Offset to shared RAM */
#define PRU_SHARED_RAM_SIZE 0x3000      /* 12KB */

/* Shared memory structure (must match PRU firmware) */
typedef struct {
    uint32_t command;       /* 0=stop, 1=blink, 2=solid on, 3=solid off */
    uint32_t period_ms;     /* blink period in milliseconds */
    uint32_t count;         /* number of blinks done (written by PRU) */
    uint32_t status;        /* 0=idle, 1=running (written by PRU) */
} PruSharedMem;

/* PRU remoteproc paths */
#define PRU0_STATE   "/sys/class/remoteproc/remoteproc1/state"
#define PRU0_FW      "/sys/class/remoteproc/remoteproc1/firmware"
#define PRU_FW_NAME  "blink_pru0.out"

static volatile int g_stop = 0;

void on_signal(int sig) { (void)sig; g_stop = 1; }

/* ----------------------------------------
 * PRU CONTROL via remoteproc
 * ---------------------------------------- */
static int pru_write_state(const char *path, const char *state)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return -1;
    }
    fprintf(f, "%s", state);
    fclose(f);
    return 0;
}

static int pru_load_firmware(void)
{
    printf("  Loading PRU firmware: %s\n", PRU_FW_NAME);

    /* Stop PRU first */
    pru_write_state(PRU0_STATE, "stop");
    usleep(100000);

    /* Set firmware name */
    pru_write_state(PRU0_FW, PRU_FW_NAME);

    /* Start PRU */
    if (pru_write_state(PRU0_STATE, "start") < 0) {
        printf("  ❌ Failed to start PRU\n");
        printf("     Make sure %s exists in /lib/firmware/\n", PRU_FW_NAME);
        return -1;
    }

    printf("  ✅ PRU started\n");
    return 0;
}

/* ----------------------------------------
 * MAP PRU SHARED MEMORY
 * ---------------------------------------- */
static PruSharedMem *map_pru_shared_mem(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem (need root)");
        return NULL;
    }

    void *base = mmap(NULL, PRUSS_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd,
                      PRUSS_BASE);
    close(fd);

    if (base == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    return (PruSharedMem *)((char *)base + PRU_SHARED_RAM_OFF);
}

/* ----------------------------------------
 * MAIN
 * ---------------------------------------- */
int main(void)
{
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   BBB PRU Blink Demo (Host Side)     ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    printf("  The PRU (Programmable Real-time Unit):\n");
    printf("  ├── Runs at 200MHz (5ns per instruction)\n");
    printf("  ├── Independent from Linux kernel\n");
    printf("  ├── Zero jitter GPIO control\n");
    printf("  └── Shared memory for ARM ↔ PRU communication\n\n");

    printf("  Hardware: Connect LED to P8_11 (GPIO0[13])\n\n");

    /* Load PRU firmware */
    if (pru_load_firmware() < 0) {
        printf("\n  Note: PRU firmware not found.\n");
        printf("  This demo needs blink_pru0.out in /lib/firmware/\n");
        printf("  See blink.asm for the PRU firmware source.\n\n");
        printf("  Running in simulation mode...\n\n");
        /* Fall through to show the concept */
    }

    /* Map shared memory */
    PruSharedMem *shm = map_pru_shared_mem();
    if (!shm) {
        printf("  Running without /dev/mem access.\n");
        printf("  On BBB with root: sudo ./blink\n\n");
    }

    /* Control loop */
    uint32_t periods[] = {100, 500, 200, 50};
    int      np        = sizeof(periods) / sizeof(periods[0]);

    printf("  Sending blink commands to PRU...\n\n");

    for (int i = 0; i < np && !g_stop; i++) {
        printf("  Blink period: %dms\n", periods[i]);

        if (shm) {
            shm->period_ms = periods[i];
            shm->command   = 1;  /* blink */
        }

        sleep(3);

        if (shm)
            printf("  PRU blink count: %u\n", shm->count);
    }

    /* Stop PRU */
    if (shm) {
        shm->command = 0;  /* stop */
    }
    pru_write_state(PRU0_STATE, "stop");

    printf("\n  PRU stopped.\n\n");
    printf("  NEXT STEPS:\n");
    printf("  1. Write blink.asm (PRU assembly firmware)\n");
    printf("  2. Compile with pasm: pasm -b blink.asm\n");
    printf("  3. Copy to /lib/firmware/blink_pru0.out\n");
    printf("  4. Run: sudo ./blink\n\n");

    return 0;
}
