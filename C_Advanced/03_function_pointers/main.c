/*
 * 03_function_pointers/main.c
 *
 * TOPIC: Function Pointers — The Heart of Firmware Architecture
 *
 * Covers:
 *   1. Basic function pointer syntax
 *   2. typedef to clean up syntax
 *   3. Callback pattern (used everywhere in drivers)
 *   4. Driver table / dispatch table (how Linux drivers work)
 *   5. State machine with function pointers
 *      → this is EXACTLY how inverter firmware works
 *
 * Build: make
 * Run:   ./function_pointers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * SECTION 1: Basic Function Pointer
 * ============================================================ */
void section1_basic(void)
{
    printf("=================================================\n");
    printf(" SECTION 1: Basic Function Pointer\n");
    printf("=================================================\n\n");

    /*
     * Normal function:
     *   int add(int a, int b) { return a + b; }
     *
     * Pointer to that function:
     *   int (*fp)(int, int);
     *     │   │    └─── parameter types
     *     │   └──────── pointer name
     *     └──────────── return type
     */

    /* Three math functions with same signature */
    int add(int a, int b);
    int sub(int a, int b);
    int mul(int a, int b);

    int (*operation)(int, int);   /* function pointer */

    operation = add;              /* point to add */
    printf("  operation = add:  %d\n", operation(10, 3));

    operation = sub;              /* point to sub */
    printf("  operation = sub:  %d\n", operation(10, 3));

    operation = mul;              /* point to mul */
    printf("  operation = mul:  %d\n\n", operation(10, 3));

    /* Function pointer in an array */
    int (*ops[3])(int, int) = { add, sub, mul };
    const char *names[] = { "add", "sub", "mul" };

    printf("  Array of function pointers:\n");
    for (int i = 0; i < 3; i++)
        printf("  ops[%d] (%s): %d\n", i, names[i], ops[i](10, 3));
    printf("\n");
}

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

/* ============================================================
 * SECTION 2: typedef — clean up the syntax
 * ============================================================ */

/* Without typedef — ugly */
/* void (*handler)(int event, void *data); */

/* With typedef — clean, readable */
typedef void (*EventHandler)(int event, void *data);
typedef int  (*ReadFunc)(uint32_t reg, uint32_t *val);
typedef int  (*WriteFunc)(uint32_t reg, uint32_t val);
typedef void (*AlertCallback)(float temperature, void *user_data);

void section2_typedef(void)
{
    printf("=================================================\n");
    printf(" SECTION 2: typedef for Function Pointers\n");
    printf("=================================================\n\n");

    printf("  Without typedef:\n");
    printf("    void (*handler)(int event, void *data);\n\n");

    printf("  With typedef:\n");
    printf("    typedef void (*EventHandler)(int event, void *data);\n");
    printf("    EventHandler handler;   ← much cleaner!\n\n");

    printf("  In firmware you'll see:\n");
    printf("    typedef int  (*ReadFunc)(uint32_t reg, uint32_t *val);\n");
    printf("    typedef int  (*WriteFunc)(uint32_t reg, uint32_t val);\n");
    printf("    typedef void (*AlertCallback)(float temp, void *ctx);\n\n");
}

/* ============================================================
 * SECTION 3: Callback Pattern
 *
 * Used in:
 *   - Linux kernel drivers (interrupt callbacks)
 *   - Your LM75 driver (probe/remove functions)
 *   - Sensor alert thresholds
 * ============================================================ */

/* Sensor driver that calls back when threshold exceeded */
typedef struct {
    float           threshold;
    AlertCallback   on_alert;
    void           *user_data;    /* passed back to callback */
} TempSensor;

static void on_temp_alert(float temp, void *user_data)
{
    const char *sensor_name = (const char *)user_data;
    printf("  🔥 ALERT from [%s]: %.1f C exceeded threshold!\n",
           sensor_name, temp);
}

static void sensor_check(TempSensor *s, float current_temp)
{
    if (current_temp >= s->threshold && s->on_alert != NULL)
        s->on_alert(current_temp, s->user_data);   /* fire callback */
}

void section3_callbacks(void)
{
    printf("=================================================\n");
    printf(" SECTION 3: Callback Pattern\n");
    printf("=================================================\n\n");

    TempSensor lm75 = {
        .threshold = 60.0f,
        .on_alert  = on_temp_alert,       /* register callback */
        .user_data = "LM75-BBB",          /* passed back in callback */
    };

    printf("  Simulating temperature readings:\n");
    float readings[] = {35.0f, 48.0f, 59.9f, 60.1f, 75.0f};
    for (int i = 0; i < 5; i++) {
        printf("  T = %.1f C  ", readings[i]);
        sensor_check(&lm75, readings[i]);
        if (readings[i] < lm75.threshold)
            printf("(normal)\n");
    }
    printf("\n");

    printf("  This pattern is EVERYWHERE:\n");
    printf("  - Linux: driver->probe(), driver->remove()\n");
    printf("  - POSIX: signal(SIGINT, my_handler)\n");
    printf("  - Firmware: fault_table[code].handler()\n\n");
}

/* ============================================================
 * SECTION 4: Driver Table / Dispatch Table
 *
 * This is how Linux file operations work (fops)
 * and how every hardware driver is structured
 * ============================================================ */

/* Generic driver interface */
typedef struct {
    const char  *name;
    int        (*init)(void);
    int        (*read)(float *value);
    int        (*write)(float value);
    void       (*deinit)(void);
} Driver;

/* LM75 implementation */
static int lm75_init(void)  { printf("    [LM75] init\n");  return 0; }
static int lm75_read(float *v) { *v = 28.5f; printf("    [LM75] read: %.1f C\n", *v); return 0; }
static int lm75_write(float v) { (void)v; printf("    [LM75] write (read-only!)\n"); return -1; }
static void lm75_deinit(void)  { printf("    [LM75] deinit\n"); }

/* BME280 implementation */
static int bme280_init(void)  { printf("    [BME280] init\n");  return 0; }
static int bme280_read(float *v) { *v = 26.1f; printf("    [BME280] read: %.1f C\n", *v); return 0; }
static int bme280_write(float v) { (void)v; return -1; }
static void bme280_deinit(void)  { printf("    [BME280] deinit\n"); }

/* Driver table — add new sensors without changing main code */
static Driver driver_table[] = {
    { "LM75",   lm75_init,   lm75_read,   lm75_write,   lm75_deinit   },
    { "BME280", bme280_init, bme280_read, bme280_write,  bme280_deinit },
};

void section4_driver_table(void)
{
    printf("=================================================\n");
    printf(" SECTION 4: Driver Table (like Linux fops)\n");
    printf("=================================================\n\n");

    int n = sizeof(driver_table) / sizeof(driver_table[0]);

    printf("  Running all drivers through same interface:\n\n");

    for (int i = 0; i < n; i++) {
        Driver *d = &driver_table[i];
        printf("  Driver: %s\n", d->name);
        d->init();
        float val = 0;
        d->read(&val);
        d->deinit();
        printf("\n");
    }

    printf("  KEY INSIGHT:\n");
    printf("  The caller doesn't know WHICH driver it's calling.\n");
    printf("  It just calls d->read() and gets the value.\n");
    printf("  This is EXACTLY how Linux kernel drivers work!\n\n");
}

/* ============================================================
 * SECTION 5: State Machine with Function Pointers
 *
 * This is HOW INVERTER FIRMWARE IS STRUCTURED
 * Each state has an entry, run, and exit function
 * ============================================================ */

typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_FAULT,
    STATE_COUNT
} InverterState;

typedef struct {
    InverterState  current_state;
    float          dc_voltage;
    float          ac_voltage;
    float          temperature;
    uint8_t        fault_code;
} InverterCtx;

/* Forward declarations */
typedef InverterState (*StateFunc)(InverterCtx *ctx);

static InverterState state_init(InverterCtx *ctx);
static InverterState state_idle(InverterCtx *ctx);
static InverterState state_starting(InverterCtx *ctx);
static InverterState state_running(InverterCtx *ctx);
static InverterState state_fault(InverterCtx *ctx);

/* State table — each state maps to a handler function */
static const StateFunc state_table[STATE_COUNT] = {
    [STATE_INIT]     = state_init,
    [STATE_IDLE]     = state_idle,
    [STATE_STARTING] = state_starting,
    [STATE_RUNNING]  = state_running,
    [STATE_FAULT]    = state_fault,
};

static const char *state_names[STATE_COUNT] = {
    "INIT", "IDLE", "STARTING", "RUNNING", "FAULT"
};

/* State implementations */
static InverterState state_init(InverterCtx *ctx)
{
    printf("  [INIT]     Checking hardware...\n");
    ctx->dc_voltage  = 385.0f;
    ctx->ac_voltage  = 0.0f;
    ctx->temperature = 32.0f;
    return STATE_IDLE;
}

static InverterState state_idle(InverterCtx *ctx)
{
    printf("  [IDLE]     DC=%.0fV, waiting for start command\n",
           ctx->dc_voltage);
    if (ctx->dc_voltage > 350.0f)
        return STATE_STARTING;
    return STATE_IDLE;
}

static InverterState state_starting(InverterCtx *ctx)
{
    printf("  [STARTING] Ramping up SPWM...\n");
    ctx->ac_voltage = 230.0f;
    if (ctx->temperature > 80.0f) {
        ctx->fault_code = 0x04;
        return STATE_FAULT;
    }
    return STATE_RUNNING;
}

static InverterState state_running(InverterCtx *ctx)
{
    printf("  [RUNNING]  AC=%.0fV, T=%.0fC ✅\n",
           ctx->ac_voltage, ctx->temperature);
    ctx->temperature += 5.0f;    /* simulate heating */
    if (ctx->temperature > 80.0f) {
        ctx->fault_code = 0x04;
        return STATE_FAULT;
    }
    return STATE_RUNNING;
}

static InverterState state_fault(InverterCtx *ctx)
{
    printf("  [FAULT]    ❌ Code=0x%02X — shutting down\n",
           ctx->fault_code);
    ctx->ac_voltage = 0.0f;
    return STATE_FAULT;   /* stay in fault until reset */
}

void section5_state_machine(void)
{
    printf("=================================================\n");
    printf(" SECTION 5: State Machine — Inverter Controller\n");
    printf("=================================================\n\n");

    printf("  State flow:\n");
    printf("  INIT → IDLE → STARTING → RUNNING → FAULT\n\n");

    InverterCtx ctx = {
        .current_state = STATE_INIT,
    };

    printf("  Running state machine:\n\n");

    for (int tick = 0; tick < 8; tick++) {
        InverterState next;

        printf("  Tick %d: [%s]\n", tick, state_names[ctx.current_state]);

        /* Call the current state's handler via function pointer */
        next = state_table[ctx.current_state](&ctx);

        ctx.current_state = next;

        if (ctx.current_state == STATE_FAULT)
            break;
    }

    printf("\n  KEY: state_table[current_state](&ctx)\n");
    printf("  No giant if-else or switch needed.\n");
    printf("  Add a new state = add one row to the table.\n\n");
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║  Function Pointers — Firmware Style  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    section1_basic();
    section2_typedef();
    section3_callbacks();
    section4_driver_table();
    section5_state_machine();

    printf("=================================================\n");
    printf(" NEXT: 04_advanced_pointers/\n");
    printf(" → double pointers, void*, memory layout\n");
    printf("=================================================\n\n");

    return 0;
}
