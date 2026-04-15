/*
 * 02_pointers_structs/main.c
 *
 * TOPIC: Pointers + Structs — The Core of Firmware Design
 *
 * Covers:
 *   1. Struct basics + pointer to struct
 *   2. Arrow operator -> vs dot operator .
 *   3. Nested structs (how real firmware is organized)
 *   4. Linked list (the most important data structure in C)
 *   5. How inverter firmware organizes its data
 *
 * Build: make
 * Run:   ./pointers_structs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * SECTION 1: Struct + Pointer basics
 *
 * dot (.)    → use when you HAVE the struct
 * arrow (->) → use when you HAVE A POINTER to the struct
 * ============================================================ */

typedef struct {
    float    voltage;
    float    current;
    float    temperature;
    uint8_t  fault_code;
} InverterData;

void section1_struct_pointer(void)
{
    printf("=================================================\n");
    printf(" SECTION 1: Struct Pointer — . vs ->\n");
    printf("=================================================\n\n");

    /* Direct struct — use dot */
    InverterData data;
    data.voltage     = 230.5f;
    data.current     = 12.3f;
    data.temperature = 45.2f;
    data.fault_code  = 0;

    printf("  Direct struct (dot operator):\n");
    printf("  data.voltage     = %.1f V\n",  data.voltage);
    printf("  data.current     = %.1f A\n",  data.current);
    printf("  data.temperature = %.1f C\n\n",data.temperature);

    /* Pointer to struct — use arrow */
    InverterData *p = &data;

    printf("  Pointer to struct (arrow operator):\n");
    printf("  p->voltage     = %.1f V\n",  p->voltage);
    printf("  p->current     = %.1f A\n",  p->current);
    printf("  p->temperature = %.1f C\n\n",p->temperature);

    /*
     * p->voltage    is EXACTLY the same as   (*p).voltage
     * Arrow = dereference + dot in one step
     */
    printf("  p->voltage == (*p).voltage == %.1f\n\n", (*p).voltage);

    /* Modifying through pointer */
    p->voltage = 231.0f;
    printf("  After p->voltage = 231.0:\n");
    printf("  data.voltage = %.1f (changed through pointer)\n\n", data.voltage);
}

/* ============================================================
 * SECTION 2: Nested Structs
 * This is how real firmware is organized
 * ============================================================ */

typedef struct {
    float    ac_voltage;
    float    ac_current;
    float    frequency;
} AcOutput;

typedef struct {
    float    dc_voltage;
    float    dc_current;
} DcInput;

typedef struct {
    float    board_temp;
    float    mosfet_temp;
    uint8_t  fan_speed_pct;
} Thermal;

typedef struct {
    uint32_t  id;
    char      name[32];
    AcOutput  ac;         /* nested struct */
    DcInput   dc;         /* nested struct */
    Thermal   thermal;    /* nested struct */
    uint8_t   fault;
} Inverter;

void section2_nested_structs(void)
{
    printf("=================================================\n");
    printf(" SECTION 2: Nested Structs\n");
    printf("=================================================\n\n");

    Inverter inv;
    memset(&inv, 0, sizeof(inv));

    inv.id                = 1;
    strcpy(inv.name,        "INV-001");
    inv.ac.ac_voltage     = 230.0f;
    inv.ac.ac_current     = 15.0f;
    inv.ac.frequency      = 50.0f;
    inv.dc.dc_voltage     = 400.0f;
    inv.dc.dc_current     = 8.7f;
    inv.thermal.board_temp  = 42.0f;
    inv.thermal.mosfet_temp = 68.0f;
    inv.thermal.fan_speed_pct = 60;

    printf("  Inverter: %s (ID=%u)\n\n", inv.name, inv.id);

    /* Accessing nested members */
    printf("  AC Output:\n");
    printf("    Voltage  : %.1f V\n",  inv.ac.ac_voltage);
    printf("    Current  : %.1f A\n",  inv.ac.ac_current);
    printf("    Frequency: %.1f Hz\n\n",inv.ac.frequency);

    printf("  DC Input:\n");
    printf("    Voltage  : %.1f V\n",  inv.dc.dc_voltage);
    printf("    Current  : %.1f A\n\n",inv.dc.dc_current);

    printf("  Thermal:\n");
    printf("    Board    : %.1f C\n",  inv.thermal.board_temp);
    printf("    MOSFET   : %.1f C\n",  inv.thermal.mosfet_temp);
    printf("    Fan      : %d%%\n\n",  inv.thermal.fan_speed_pct);

    /* Pointer to nested struct member */
    Inverter *p  = &inv;
    Thermal  *pt = &p->thermal;   /* pointer directly to nested struct */

    pt->fan_speed_pct = 80;
    printf("  Changed fan via nested pointer: %d%%\n\n",
           inv.thermal.fan_speed_pct);

    printf("  sizeof(Inverter) = %zu bytes\n\n", sizeof(Inverter));
}

/* ============================================================
 * SECTION 3: Array of Structs + Pointers
 * Used in firmware for: fault logs, sensor tables, config tables
 * ============================================================ */

typedef struct {
    uint8_t  code;
    char     description[48];
    uint8_t  severity;    /* 1=warning, 2=error, 3=critical */
} FaultEntry;

static FaultEntry fault_table[] = {
    { 0x01, "DC undervoltage",         2 },
    { 0x02, "DC overvoltage",          3 },
    { 0x03, "AC output overcurrent",   3 },
    { 0x04, "Overtemperature",         2 },
    { 0x05, "Grid frequency out range",1 },
};

void section3_array_of_structs(void)
{
    printf("=================================================\n");
    printf(" SECTION 3: Array of Structs\n");
    printf("=================================================\n\n");

    int n = sizeof(fault_table) / sizeof(fault_table[0]);

    printf("  Fault Table (%d entries):\n\n", n);
    printf("  Code  Severity  Description\n");
    printf("  ────  ────────  ──────────────────────────\n");

    /* Method 1: index */
    for (int i = 0; i < n; i++) {
        const char *sev = fault_table[i].severity == 3 ? "CRITICAL" :
                          fault_table[i].severity == 2 ? "ERROR   " : "WARNING ";
        printf("  0x%02X  %s  %s\n",
               fault_table[i].code,
               sev,
               fault_table[i].description);
    }

    /* Method 2: pointer walk — same result, used in firmware */
    printf("\n  Same with pointer walk:\n");
    FaultEntry *p = fault_table;
    FaultEntry *end = fault_table + n;
    while (p < end) {
        printf("  0x%02X → %s\n", p->code, p->description);
        p++;    /* moves to next FaultEntry (jumps sizeof(FaultEntry) bytes) */
    }

    /* Search by fault code — firmware pattern */
    uint8_t search = 0x04;
    FaultEntry *found = NULL;
    for (FaultEntry *q = fault_table; q < fault_table + n; q++) {
        if (q->code == search) {
            found = q;
            break;
        }
    }
    printf("\n  Search for 0x04: %s\n\n",
           found ? found->description : "not found");
}

/* ============================================================
 * SECTION 4: Linked List
 *
 * Used in firmware for: event queues, free memory pools,
 * dynamic sensor lists, command queues
 * ============================================================ */

typedef struct Node {
    float        value;
    uint32_t     timestamp;
    struct Node *next;    /* pointer to NEXT node of same type */
} Node;

/* Create a new node on heap */
static Node *node_create(float value, uint32_t ts)
{
    Node *n = malloc(sizeof(Node));
    if (!n) return NULL;
    n->value     = value;
    n->timestamp = ts;
    n->next      = NULL;
    return n;
}

/* Append to end of list */
static void list_append(Node **head, float value, uint32_t ts)
{
    Node *new_node = node_create(value, ts);
    if (!new_node) return;

    if (*head == NULL) {
        *head = new_node;
        return;
    }

    /* Walk to end */
    Node *cur = *head;
    while (cur->next != NULL)
        cur = cur->next;
    cur->next = new_node;
}

/* Print list */
static void list_print(Node *head)
{
    Node *cur = head;
    int   i   = 0;
    while (cur != NULL) {
        printf("  [%d] t=%u  val=%.1f\n", i++, cur->timestamp, cur->value);
        cur = cur->next;
    }
}

/* Free entire list */
static void list_free(Node **head)
{
    Node *cur = *head;
    while (cur != NULL) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    *head = NULL;
}

void section4_linked_list(void)
{
    printf("=================================================\n");
    printf(" SECTION 4: Linked List (Temperature Log)\n");
    printf("=================================================\n\n");

    /*
     * head → [25.1|t=0|*] → [26.3|t=1|*] → [27.0|t=2|NULL]
     *
     * Each node contains data + pointer to next node
     * Last node's next = NULL (end of list)
     */

    Node *head = NULL;   /* empty list */

    /* Add temperature readings */
    list_append(&head, 25.1f, 0);
    list_append(&head, 26.3f, 1);
    list_append(&head, 27.0f, 2);
    list_append(&head, 45.8f, 3);   /* spike! */
    list_append(&head, 28.1f, 4);

    printf("  Temperature log (linked list):\n");
    list_print(head);

    /* Why **head (double pointer)?
     * list_append needs to MODIFY the head pointer itself
     * if list was empty. Passing *head lets us change
     * what head POINTS TO from inside the function.
     * → Covered in detail in 04_advanced_pointers
     */
    printf("\n  head pointer = %p\n\n", (void*)head);

    list_free(&head);
    printf("  After free: head = %p (NULL)\n\n", (void*)head);
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║  Pointers + Structs — Firmware Style ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    section1_struct_pointer();
    section2_nested_structs();
    section3_array_of_structs();
    section4_linked_list();

    printf("=================================================\n");
    printf(" NEXT: 03_function_pointers/\n");
    printf(" → callbacks, driver tables, state machines\n");
    printf(" → this is how inverter firmware is structured\n");
    printf("=================================================\n\n");

    return 0;
}
