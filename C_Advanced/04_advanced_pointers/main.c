/*
 * 04_advanced_pointers/main.c
 *
 * TOPIC: Advanced Pointers
 *
 * Covers:
 *   1. Double pointers (**) — pointer to pointer
 *   2. void* — generic pointer (used in malloc, memcpy, drivers)
 *   3. Memory layout: stack vs heap vs data vs BSS
 *   4. restrict keyword (performance hint to compiler)
 *   5. Pointer casting in hardware register access
 *      → exactly how you access MCU registers in firmware
 *
 * Build: make
 * Run:   ./advanced_pointers
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * SECTION 1: Double Pointer — pointer to pointer
 *
 * When do you need **?
 *   - When a function needs to MODIFY a pointer
 *   - Dynamic 2D arrays
 *   - Linked list head modification
 * ============================================================ */

void bad_alloc(int *p, int size)
{
    p = malloc(size * sizeof(int));   /* only modifies LOCAL copy of p */
    (void)p;
    /* caller's pointer is NOT changed — memory leaked! */
}

void good_alloc(int **p, int size)
{
    *p = malloc(size * sizeof(int));  /* modifies the ACTUAL pointer */
}

void section1_double_pointer(void)
{
    printf("=================================================\n");
    printf(" SECTION 1: Double Pointer (**)\n");
    printf("=================================================\n\n");

    /*
     * int x = 5;
     * int *p  = &x;    p  stores address of x
     * int **pp = &p;   pp stores address of p
     *
     * Memory:
     *   x  [  5  ]  addr: 0x100
     *   p  [0x100]  addr: 0x200   ← p points to x
     *   pp [0x200]  addr: 0x300   ← pp points to p
     *
     *   *pp  = p  = 0x100
     *   **pp = x  = 5
     */

    int  x  = 5;
    int *p  = &x;
    int **pp = &p;

    printf("  x   = %d\n",  x);
    printf("  *p  = %d\n",  *p);
    printf("  **pp= %d\n\n",**pp);

    printf("  &x  = %p\n",  (void*)&x);
    printf("  p   = %p  (points to x)\n",   (void*)p);
    printf("  *pp = %p  (same as p)\n",      (void*)*pp);
    printf("  pp  = %p  (points to p)\n\n",  (void*)pp);

    /* Modify x through pp */
    **pp = 99;
    printf("  After **pp = 99: x = %d\n\n", x);

    /* Why you need ** for allocation */
    printf("  Allocation example:\n");
    int *arr = NULL;

    bad_alloc(arr, 10);
    printf("  bad_alloc:  arr = %p (still NULL!)\n", (void*)arr);

    good_alloc(&arr, 10);
    printf("  good_alloc: arr = %p (now allocated)\n\n", (void*)arr);

    if (arr) {
        for (int i = 0; i < 10; i++) arr[i] = i * 10;
        printf("  arr[5] = %d\n\n", arr[5]);
        free(arr);
    }
}

/* ============================================================
 * SECTION 2: void* — The Generic Pointer
 *
 * void* can hold address of ANY type
 * Must cast before dereferencing
 * Used in: malloc, memcpy, qsort, kernel driver private data
 * ============================================================ */

/* Generic swap — works for any type */
void generic_swap(void *a, void *b, size_t size)
{
    /* Use stack buffer for temp */
    uint8_t temp[256];
    if (size > sizeof(temp)) return;

    memcpy(temp, a, size);
    memcpy(a,    b, size);
    memcpy(b, temp, size);
}

/* Generic print — figure out type at runtime */
typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_CHAR } DataType;

void print_value(void *ptr, DataType type)
{
    switch (type) {
    case TYPE_INT:   printf("%d",   *(int*)ptr);   break;
    case TYPE_FLOAT: printf("%.2f", *(float*)ptr); break;
    case TYPE_CHAR:  printf("%c",   *(char*)ptr);  break;
    }
}

/* How Linux kernel stores driver private data */
typedef struct {
    void    *private_data;    /* void* = any driver can store anything here */
    uint32_t flags;
} Device;

void section2_void_pointer(void)
{
    printf("=================================================\n");
    printf(" SECTION 2: void* — Generic Pointer\n");
    printf("=================================================\n\n");

    /* Generic swap */
    int   a = 10, b = 20;
    float x = 1.5f, y = 2.5f;

    generic_swap(&a, &b, sizeof(int));
    printf("  After swap: a=%d b=%d\n", a, b);

    generic_swap(&x, &y, sizeof(float));
    printf("  After swap: x=%.1f y=%.1f\n\n", x, y);

    /* void* to any type */
    void *ptr;
    int    i = 42;
    float  f = 3.14f;
    char   c = 'Z';

    ptr = &i; print_value(ptr, TYPE_INT);   printf(" (int)\n");
    ptr = &f; print_value(ptr, TYPE_FLOAT); printf(" (float)\n");
    ptr = &c; print_value(ptr, TYPE_CHAR);  printf(" (char)\n\n");

    /* Linux driver private data pattern */
    typedef struct { float voltage; float current; } InvPrivate;
    InvPrivate priv = { 230.0f, 15.0f };

    Device dev;
    dev.private_data = &priv;   /* store ANY struct as void* */

    /* Later, retrieve and cast back */
    InvPrivate *p = (InvPrivate*)dev.private_data;
    printf("  Driver private data: V=%.0f A=%.0f\n\n",
           p->voltage, p->current);
}

/* ============================================================
 * SECTION 3: Memory Layout
 *
 * Every C program has 4 memory regions.
 * Firmware engineers MUST understand this.
 * ============================================================ */

int    global_init   = 100;      /* DATA segment — initialized globals */
int    global_uninit;            /* BSS segment  — zero-initialized globals */
static int static_var = 42;      /* DATA segment — static local */

void section3_memory_layout(void)
{
    printf("=================================================\n");
    printf(" SECTION 3: Memory Layout\n");
    printf("=================================================\n\n");

    int   stack_var  = 5;        /* STACK — local variables */
    int  *heap_var   = malloc(sizeof(int));  /* HEAP — dynamic */
    *heap_var = 99;

    printf("  Memory regions:\n\n");
    printf("  ┌────────────────────────────────────────┐\n");
    printf("  │  TEXT (code)  — read only              │\n");
    printf("  │  your compiled functions live here     │\n");
    printf("  ├────────────────────────────────────────┤\n");
    printf("  │  DATA — initialized globals            │\n");
    printf("  │  global_init = %d  @ %p     │\n", global_init, (void*)&global_init);
    printf("  │  static_var  = %d  @ %p     │\n", static_var, (void*)&static_var);
    printf("  ├────────────────────────────────────────┤\n");
    printf("  │  BSS — zero-initialized globals        │\n");
    printf("  │  global_uninit = %d   @ %p  │\n", global_uninit, (void*)&global_uninit);
    printf("  ├────────────────────────────────────────┤\n");
    printf("  │  HEAP — grows upward (malloc)          │\n");
    printf("  │  heap_var = %d  @ %p     │\n", *heap_var, (void*)heap_var);
    printf("  ├────────────────────────────────────────┤\n");
    printf("  │  STACK — grows downward (local vars)   │\n");
    printf("  │  stack_var = %d  @ %p     │\n", stack_var, (void*)&stack_var);
    printf("  └────────────────────────────────────────┘\n\n");

    printf("  In FIRMWARE (MCU):\n");
    printf("  TEXT   → Flash memory (read-only)\n");
    printf("  DATA   → RAM (copied from Flash on boot)\n");
    printf("  BSS    → RAM (zeroed on boot)\n");
    printf("  HEAP   → avoid in RT firmware (unpredictable timing)\n");
    printf("  STACK  → RAM (each task gets its own stack)\n\n");

    free(heap_var);
}

/* ============================================================
 * SECTION 4: Hardware Register Access via Pointers
 *
 * This is the FOUNDATION of all MCU/embedded programming.
 * Every peripheral register is just a memory address.
 * You access it by casting to a pointer.
 * ============================================================ */

/* Simulated hardware registers (in real firmware these are fixed addresses) */
static volatile uint32_t FAKE_GPIO_DIR    = 0x00000000;
static volatile uint32_t FAKE_GPIO_OUT    = 0x00000000;
static volatile uint32_t FAKE_GPIO_IN     = 0xDEADBEEF;

void section4_register_access(void)
{
    printf("=================================================\n");
    printf(" SECTION 4: Hardware Register Access\n");
    printf("=================================================\n\n");

    /*
     * On a real MCU/BBB:
     *   #define GPIO0_BASE  0x44E07000
     *   #define GPIO_OE     (*(volatile uint32_t*)(GPIO0_BASE + 0x134))
     *   #define GPIO_SETOUT (*(volatile uint32_t*)(GPIO0_BASE + 0x194))
     *
     * volatile = tells compiler: don't optimize this away,
     *            hardware can change this value anytime
     */

    printf("  In real firmware you write:\n\n");
    printf("  #define GPIO0_BASE  0x44E07000\n");
    printf("  #define GPIO_OE     (*(volatile uint32_t*)(GPIO0_BASE + 0x134))\n\n");
    printf("  GPIO_OE &= ~(1 << 13);   // set P8_11 as output\n");
    printf("  GPIO_SETOUT |= (1 << 13); // set P8_11 HIGH\n\n");

    /* Simulate with our fake registers */
    volatile uint32_t *dir = &FAKE_GPIO_DIR;
    volatile uint32_t *out = &FAKE_GPIO_OUT;
    volatile uint32_t *in  = &FAKE_GPIO_IN;

    /* Set bit 13 as output */
    *dir &= ~(1u << 13);
    printf("  *dir after setting bit 13 output: 0x%08X\n", *dir);

    /* Set bit 13 HIGH */
    *out |= (1u << 13);
    printf("  *out after setting bit 13 HIGH:   0x%08X\n\n", *out);

    /* Read input register */
    uint32_t val = *in;
    printf("  GPIO input register: 0x%08X\n", val);
    printf("  Bit 5 state: %d\n\n", (val >> 5) & 1);

    printf("  volatile keyword = CRITICAL for registers\n");
    printf("  Without it: compiler may cache the value and\n");
    printf("  never re-read from hardware!\n\n");
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   Advanced Pointers                  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    section1_double_pointer();
    section2_void_pointer();
    section3_memory_layout();
    section4_register_access();

    printf("=================================================\n");
    printf(" NEXT: 05_firmware_design/\n");
    printf(" → put it all together: mini inverter controller\n");
    printf("=================================================\n\n");

    return 0;
}
