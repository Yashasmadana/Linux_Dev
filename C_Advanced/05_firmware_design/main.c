/*
 * 05_firmware_design/main.c
 *
 * TOPIC: Mini Inverter Controller — Everything Combined
 *
 * This is a simplified version of how REAL inverter firmware
 * is designed. Uses ALL concepts from previous modules:
 *
 *   ✅ Structs + nested structs  (data organisation)
 *   ✅ Pointers + arrow operator (accessing hardware data)
 *   ✅ Function pointers         (state machine + driver table)
 *   ✅ Callbacks                 (fault + alert handling)
 *   ✅ void*                     (generic driver interface)
 *   ✅ volatile registers        (hardware access simulation)
 *   ✅ const pointers            (read-only config)
 *   ✅ Double pointers           (dynamic lists)
 *
 * Architecture:
 *
 *   main()
 *     └── inverter_run()
 *           ├── sensor_layer   (reads voltage, current, temp)
 *           ├── protection     (overvoltage, overtemp, overcurrent)
 *           ├── state_machine  (INIT→IDLE→RUNNING→FAULT)
 *           └── fault_manager  (log, callback, shutdown)
 *
 * Build: make
 * Run:   ./firmware_design
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ============================================================
 * 1. HARDWARE ABSTRACTION LAYER (HAL)
 *    Simulated registers — in real firmware these are
 *    memory-mapped to actual peripheral addresses
 * ============================================================ */

typedef struct {
    volatile uint32_t dc_voltage_raw;     /* ADC counts */
    volatile uint32_t dc_current_raw;
    volatile uint32_t ac_voltage_raw;
    volatile uint32_t temperature_raw;
    volatile uint32_t fault_status;
    volatile uint32_t control_reg;        /* bit0=enable, bit1=fan */
} HardwareRegs;

/* Simulated hardware */
static HardwareRegs hw_regs = {
    .dc_voltage_raw  = 3200,   /* ~400V */
    .dc_current_raw  = 700,    /* ~8.75A */
    .ac_voltage_raw  = 1840,   /* ~230V */
    .temperature_raw = 420,    /* ~42C */
    .fault_status    = 0,
    .control_reg     = 0,
};

/* HAL conversion functions */
static float hal_read_dc_voltage(void)
{
    return (float)hw_regs.dc_voltage_raw * 0.125f;
}
static float hal_read_dc_current(void)
{
    return (float)hw_regs.dc_current_raw * 0.0125f;
}
static float hal_read_ac_voltage(void)
{
    return (float)hw_regs.ac_voltage_raw * 0.125f;
}
static float hal_read_temperature(void)
{
    return (float)hw_regs.temperature_raw * 0.1f;
}
static void hal_set_output_enable(int en)
{
    if (en) hw_regs.control_reg |=  (1u << 0);
    else    hw_regs.control_reg &= ~(1u << 0);
}
static void hal_set_fan(int en)
{
    if (en) hw_regs.control_reg |=  (1u << 1);
    else    hw_regs.control_reg &= ~(1u << 1);
}

/* ============================================================
 * 2. DATA STRUCTURES
 * ============================================================ */

/* Fault codes */
#define FAULT_NONE          0x00
#define FAULT_DC_UNDER      0x01
#define FAULT_DC_OVER       0x02
#define FAULT_AC_OVER       0x03
#define FAULT_OVERTEMP      0x04
#define FAULT_OVERCURRENT   0x05

/* Protection thresholds */
typedef struct {
    float dc_voltage_min;
    float dc_voltage_max;
    float ac_voltage_max;
    float temperature_max;
    float current_max;
} ProtectionLimits;

/* Live measurements */
typedef struct {
    float dc_voltage;
    float dc_current;
    float ac_voltage;
    float temperature;
    float power_kw;
} Measurements;

/* Fault log entry */
typedef struct {
    uint8_t  code;
    float    value;           /* value that triggered fault */
    uint32_t tick;
    struct FaultLogEntry *next;
} FaultLogEntry;

/* Inverter state */
typedef enum {
    INV_STATE_INIT,
    INV_STATE_IDLE,
    INV_STATE_STARTING,
    INV_STATE_RUNNING,
    INV_STATE_STOPPING,
    INV_STATE_FAULT,
    INV_STATE_COUNT
} InvState;

/* Fault callback type */
typedef void (*FaultCallback)(uint8_t code, float value, void *ctx);

/* Main inverter context — the central data structure */
typedef struct {
    /* Identity */
    uint32_t          id;
    char              name[32];

    /* State */
    InvState          state;
    uint32_t          tick;

    /* Data */
    Measurements      meas;
    ProtectionLimits  limits;

    /* Faults */
    uint8_t           active_fault;
    FaultLogEntry    *fault_log;          /* linked list of faults */
    FaultCallback     on_fault;           /* fault callback */
    void             *fault_ctx;          /* callback context */

    /* Stats */
    uint32_t          run_ticks;
    float             total_kwh;
} Inverter;

/* ============================================================
 * 3. FAULT MANAGER
 * ============================================================ */

static void fault_log_add(Inverter *inv, uint8_t code, float value)
{
    FaultLogEntry *entry = malloc(sizeof(FaultLogEntry));
    if (!entry) return;

    entry->code  = code;
    entry->value = value;
    entry->tick  = inv->tick;
    entry->next  = NULL;

    /* Prepend to list */
    entry->next  = (struct FaultLogEntry*)inv->fault_log;
    inv->fault_log = entry;
}

static void fault_trigger(Inverter *inv, uint8_t code, float value)
{
    if (inv->active_fault != FAULT_NONE) return;  /* already faulted */

    inv->active_fault = code;
    fault_log_add(inv, code, value);

    /* Fire callback if registered */
    if (inv->on_fault)
        inv->on_fault(code, value, inv->fault_ctx);
}

static void fault_log_print(const Inverter *inv)
{
    printf("  Fault Log:\n");
    FaultLogEntry *e = inv->fault_log;
    if (!e) { printf("    (no faults)\n"); return; }

    const char *names[] = {
        "NONE", "DC_UNDER", "DC_OVER", "AC_OVER",
        "OVERTEMP", "OVERCURRENT"
    };

    while (e) {
        printf("    [t=%u] code=0x%02X (%s) val=%.1f\n",
               e->tick, e->code,
               e->code < 6 ? names[e->code] : "?",
               e->value);
        e = (FaultLogEntry*)e->next;
    }
}

static void fault_log_free(Inverter *inv)
{
    FaultLogEntry *e = inv->fault_log;
    while (e) {
        FaultLogEntry *next = (FaultLogEntry*)e->next;
        free(e);
        e = next;
    }
    inv->fault_log = NULL;
}

/* ============================================================
 * 4. PROTECTION MODULE
 * ============================================================ */

static uint8_t protection_check(Inverter *inv)
{
    Measurements *m = &inv->meas;
    ProtectionLimits *l = &inv->limits;

    if (m->dc_voltage < l->dc_voltage_min) {
        fault_trigger(inv, FAULT_DC_UNDER, m->dc_voltage);
        return FAULT_DC_UNDER;
    }
    if (m->dc_voltage > l->dc_voltage_max) {
        fault_trigger(inv, FAULT_DC_OVER, m->dc_voltage);
        return FAULT_DC_OVER;
    }
    if (m->temperature > l->temperature_max) {
        fault_trigger(inv, FAULT_OVERTEMP, m->temperature);
        return FAULT_OVERTEMP;
    }
    if (m->dc_current > l->current_max) {
        fault_trigger(inv, FAULT_OVERCURRENT, m->dc_current);
        return FAULT_OVERCURRENT;
    }
    return FAULT_NONE;
}

/* ============================================================
 * 5. STATE MACHINE
 * ============================================================ */

typedef InvState (*StateHandler)(Inverter *inv);

static InvState state_init(Inverter *inv)
{
    printf("  [INIT]     Initializing %s\n", inv->name);
    hal_set_output_enable(0);
    hal_set_fan(0);
    return INV_STATE_IDLE;
}

static InvState state_idle(Inverter *inv)
{
    printf("  [IDLE]     DC=%.0fV  T=%.0fC\n",
           inv->meas.dc_voltage, inv->meas.temperature);

    if (inv->meas.dc_voltage > inv->limits.dc_voltage_min)
        return INV_STATE_STARTING;

    return INV_STATE_IDLE;
}

static InvState state_starting(Inverter *inv)
{
    printf("  [STARTING] Ramping output...\n");
    hal_set_fan(1);
    hal_set_output_enable(1);

    if (protection_check(inv) != FAULT_NONE)
        return INV_STATE_FAULT;

    return INV_STATE_RUNNING;
}

static InvState state_running(Inverter *inv)
{
    /* Update power calculation */
    inv->meas.power_kw = (inv->meas.dc_voltage * inv->meas.dc_current) / 1000.0f;
    inv->total_kwh    += inv->meas.power_kw / 3600.0f;
    inv->run_ticks++;

    printf("  [RUNNING]  DC=%.0fV/%.1fA  AC=%.0fV  T=%.0fC  P=%.2fkW\n",
           inv->meas.dc_voltage, inv->meas.dc_current,
           inv->meas.ac_voltage, inv->meas.temperature,
           inv->meas.power_kw);

    /* Simulate temperature rise */
    hw_regs.temperature_raw += 30;

    /* Check protection */
    if (protection_check(inv) != FAULT_NONE)
        return INV_STATE_FAULT;

    return INV_STATE_RUNNING;
}

static InvState state_stopping(Inverter *inv)
{
    printf("  [STOPPING] Shutting down gracefully\n");
    hal_set_output_enable(0);
    hal_set_fan(0);
    (void)inv;
    return INV_STATE_IDLE;
}

static InvState state_fault(Inverter *inv)
{
    hal_set_output_enable(0);
    printf("  [FAULT]    ❌ Code=0x%02X — output disabled\n",
           inv->active_fault);
    return INV_STATE_FAULT;
}

/* State table — function pointer array */
static const StateHandler state_table[INV_STATE_COUNT] = {
    [INV_STATE_INIT]     = state_init,
    [INV_STATE_IDLE]     = state_idle,
    [INV_STATE_STARTING] = state_starting,
    [INV_STATE_RUNNING]  = state_running,
    [INV_STATE_STOPPING] = state_stopping,
    [INV_STATE_FAULT]    = state_fault,
};

/* ============================================================
 * 6. SENSOR UPDATE
 * ============================================================ */

static void sensors_update(Inverter *inv)
{
    inv->meas.dc_voltage  = hal_read_dc_voltage();
    inv->meas.dc_current  = hal_read_dc_current();
    inv->meas.ac_voltage  = hal_read_ac_voltage();
    inv->meas.temperature = hal_read_temperature();
}

/* ============================================================
 * 7. FAULT CALLBACK (registered by application layer)
 * ============================================================ */

static void app_fault_handler(uint8_t code, float value, void *ctx)
{
    const char *inv_name = (const char*)ctx;
    printf("\n  *** FAULT CALLBACK: [%s] code=0x%02X val=%.1f ***\n\n",
           inv_name, code, value);
}

/* ============================================================
 * 8. MAIN LOOP
 * ============================================================ */

static void inverter_run(Inverter *inv, int max_ticks)
{
    static const char *state_names[] = {
        "INIT","IDLE","STARTING","RUNNING","STOPPING","FAULT"
    };

    for (int t = 0; t < max_ticks; t++) {
        inv->tick = t;

        /* 1. Read all sensors */
        sensors_update(inv);

        /* 2. Fan control based on temperature */
        if (inv->meas.temperature > 50.0f)
            hal_set_fan(1);

        /* 3. Run state machine via function pointer */
        InvState next = state_table[inv->state](inv);

        /* 4. State transition */
        if (next != inv->state) {
            printf("  → State: %s → %s\n\n",
                   state_names[inv->state],
                   state_names[next]);
            inv->state = next;
        }

        /* Stop if stuck in fault */
        if (inv->state == INV_STATE_FAULT)
            break;
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║   Mini Inverter Controller Simulation   ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    /* Initialize inverter */
    Inverter inv;
    memset(&inv, 0, sizeof(inv));

    inv.id    = 1;
    strcpy(inv.name, "FIMER-INV-001");
    inv.state = INV_STATE_INIT;

    /* Set protection limits */
    inv.limits.dc_voltage_min = 300.0f;
    inv.limits.dc_voltage_max = 500.0f;
    inv.limits.ac_voltage_max = 260.0f;
    inv.limits.temperature_max = 80.0f;
    inv.limits.current_max     = 20.0f;

    /* Register fault callback */
    inv.on_fault  = app_fault_handler;
    inv.fault_ctx = inv.name;

    printf("  Inverter: %s\n", inv.name);
    printf("  Limits: DC %.0f-%.0fV, Tmax=%.0fC, Imax=%.0fA\n\n",
           inv.limits.dc_voltage_min, inv.limits.dc_voltage_max,
           inv.limits.temperature_max, inv.limits.current_max);

    printf("  Running simulation (10 ticks):\n\n");

    /* Run for 10 ticks */
    inverter_run(&inv, 10);

    /* Print summary */
    printf("\n  ─────────────────────────────────\n");
    printf("  Summary:\n");
    printf("  Run ticks  : %u\n",    inv.run_ticks);
    printf("  Total kWh  : %.4f\n",  inv.total_kwh);
    printf("  Final state: %s\n",
           inv.state == INV_STATE_FAULT ? "FAULT" :
           inv.state == INV_STATE_RUNNING ? "RUNNING" : "OTHER");
    printf("  Active fault: 0x%02X\n\n", inv.active_fault);

    fault_log_print(&inv);

    printf("\n  Design patterns used:\n");
    printf("  ✅ Nested structs     → organized data\n");
    printf("  ✅ Function pointers  → state machine table\n");
    printf("  ✅ Callbacks          → fault notification\n");
    printf("  ✅ void*              → generic context passing\n");
    printf("  ✅ Linked list        → fault log\n");
    printf("  ✅ volatile registers → HAL hardware access\n");
    printf("  ✅ const pointers     → read-only limits\n\n");

    fault_log_free(&inv);

    return 0;
}
