/*
 * 01_pointers_basics/main.c
 *
 * TOPIC: Pointers — From Zero
 *
 * Covers:
 *   1. What is a pointer (address vs value)
 *   2. & (reference)   — get the address OF something
 *   3. * (dereference) — get the VALUE AT an address
 *   4. Pointer arithmetic
 *   5. Pointers and arrays (they are the same thing)
 *   6. Pointers and strings
 *   7. Common mistakes
 *
 * Build: make
 * Run:   ./pointers_basics
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * SECTION 1: What is a pointer?
 *
 * Every variable lives somewhere in RAM.
 * That location has an ADDRESS (like a house number).
 * A POINTER is a variable that STORES an address.
 * ============================================================ */
void section1_what_is_a_pointer(void)
{
    printf("=================================================\n");
    printf(" SECTION 1: What is a pointer?\n");
    printf("=================================================\n\n");

    int x = 42;         /* normal variable — stores a VALUE */
    int *p = &x;        /* pointer — stores the ADDRESS of x */

    /*
     * Memory layout:
     *
     *  Address:  0x1000   0x1004
     *            ┌──────┐ ┌──────────┐
     *  Variable: │  42  │ │ 0x1000   │
     *            └──────┘ └──────────┘
     *              x           p
     *
     *  x = the value 42
     *  p = the address where x lives
     */

    printf("  x        = %d      (the value)\n",   x);
    printf("  &x       = %p  (address OF x)\n",    (void*)&x);
    printf("  p        = %p  (p stores address of x)\n", (void*)p);
    printf("  *p       = %d      (value AT the address p holds)\n\n", *p);

    /* & means "address of" */
    printf("  &  means: give me the ADDRESS of this variable\n");
    printf("  *  means: give me the VALUE at this address\n\n");

    /* Modify x through the pointer */
    *p = 100;           /* dereference p, write 100 there */
    printf("  After *p = 100:\n");
    printf("  x = %d  (x changed even though we used p!)\n\n", x);
}

/* ============================================================
 * SECTION 2: Pointer types — size matters
 * ============================================================ */
void section2_pointer_types(void)
{
    printf("=================================================\n");
    printf(" SECTION 2: Pointer Types\n");
    printf("=================================================\n\n");

    char   c = 'A';
    int    i = 1000;
    float  f = 3.14f;
    double d = 2.718;

    char   *pc = &c;
    int    *pi = &i;
    float  *pf = &f;
    double *pd = &d;

    printf("  Type     Size    Pointer points to N bytes\n");
    printf("  ──────── ─────   ──────────────────────────\n");
    printf("  char     %zu byte   pc = %p\n",   sizeof(char),   (void*)pc);
    printf("  int      %zu bytes  pi = %p\n",   sizeof(int),    (void*)pi);
    printf("  float    %zu bytes  pf = %p\n",   sizeof(float),  (void*)pf);
    printf("  double   %zu bytes  pd = %p\n\n", sizeof(double), (void*)pd);

    printf("  Key: the POINTER itself is always the same size (%zu bytes on this CPU)\n",
           sizeof(void*));
    printf("  But it tells the CPU how many bytes to READ when dereferenced\n\n");

    printf("  *pc = '%c'\n",  *pc);
    printf("  *pi = %d\n",    *pi);
    printf("  *pf = %.2f\n",  *pf);
    printf("  *pd = %.3f\n\n",*pd);
}

/* ============================================================
 * SECTION 3: Pointer Arithmetic
 *
 * When you do p+1, it does NOT add 1 byte.
 * It adds sizeof(*p) bytes — moves to the NEXT element.
 * ============================================================ */
void section3_pointer_arithmetic(void)
{
    printf("=================================================\n");
    printf(" SECTION 3: Pointer Arithmetic\n");
    printf("=================================================\n\n");

    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;   /* p points to arr[0] */

    printf("  Array in memory:\n");
    printf("  Index:   [0]    [1]    [2]    [3]    [4]\n");
    printf("  Value:   10     20     30     40     50\n");
    printf("  Addr:    p      p+1    p+2    p+3    p+4\n\n");

    printf("  p    points to arr[0] = %d  (addr: %p)\n", *p,     (void*)p);
    printf("  p+1  points to arr[1] = %d  (addr: %p)\n", *(p+1), (void*)(p+1));
    printf("  p+2  points to arr[2] = %d  (addr: %p)\n", *(p+2), (void*)(p+2));
    printf("  p+3  points to arr[3] = %d  (addr: %p)\n", *(p+3), (void*)(p+3));
    printf("  p+4  points to arr[4] = %d  (addr: %p)\n\n", *(p+4), (void*)(p+4));

    printf("  Address difference p+1 - p = %ld bytes (= sizeof(int))\n\n",
           (char*)(p+1) - (char*)p);

    /* Walking through array with pointer */
    printf("  Walking array with pointer:\n  ");
    for (int *q = arr; q < arr + 5; q++)
        printf("%d ", *q);
    printf("\n\n");

    /* Pointer subtraction — distance between pointers */
    int *start = &arr[0];
    int *end   = &arr[4];
    printf("  end - start = %ld elements apart\n\n", end - start);
}

/* ============================================================
 * SECTION 4: Arrays and Pointers — same thing
 * ============================================================ */
void section4_arrays_and_pointers(void)
{
    printf("=================================================\n");
    printf(" SECTION 4: Arrays == Pointers\n");
    printf("=================================================\n\n");

    int arr[4] = {1, 2, 3, 4};

    /*
     * arr    == &arr[0]   (array name IS a pointer to first element)
     * arr[i] == *(arr+i)  (indexing IS pointer arithmetic)
     */

    printf("  arr[0]   = %d\n",  arr[0]);
    printf("  *arr     = %d\n",  *arr);         /* same thing */
    printf("  arr[2]   = %d\n",  arr[2]);
    printf("  *(arr+2) = %d\n\n",*(arr+2));      /* same thing */

    /* Passing array to function — always passed as pointer */
    printf("  sizeof(arr) here          = %zu bytes\n", sizeof(arr));

    /* THIS IS A CLASSIC MISTAKE: */
    /* void func(int arr[]) { sizeof(arr) == sizeof(pointer) NOT array } */
    printf("  sizeof(int*) (pointer)    = %zu bytes\n\n", sizeof(int*));
    printf("  WARNING: sizeof(arr) inside a function gives pointer size!\n");
    printf("  Always pass the length separately: func(arr, 4)\n\n");
}

/* ============================================================
 * SECTION 5: const with pointers — 3 different meanings
 * ============================================================ */
void section5_const_pointers(void)
{
    printf("=================================================\n");
    printf(" SECTION 5: const with Pointers\n");
    printf("=================================================\n\n");

    int x = 10;
    int y = 20;

    /* 1. Pointer to const — can't change the value, can move pointer */
    const int *p1 = &x;
    /* *p1 = 99;  ← ERROR: can't modify value */
    p1 = &y;      /* OK: can point elsewhere */
    printf("  const int *p1    → can't change *p1, CAN change p1\n");
    printf("  *p1 = %d\n\n", *p1);

    /* 2. Const pointer — can change value, can't move pointer */
    int * const p2 = &x;
    *p2 = 99;     /* OK: can modify value */
    /* p2 = &y;   ← ERROR: can't point elsewhere */
    printf("  int * const p2   → CAN change *p2, can't change p2\n");
    printf("  x is now %d\n\n", x);

    /* 3. Const pointer to const — can't do either */
    const int * const p3 = &y;
    (void)p3;     /* suppress unused warning */
    printf("  const int *const p3 → can't change anything\n\n");

    /*
     * In firmware you'll see:
     *   void write_register(const uint32_t *addr, uint32_t val)
     *                        ^^^^^ means we won't modify what addr points to
     */
    printf("  In firmware: const int *addr means 'read-only register'\n\n");
}

/* ============================================================
 * SECTION 6: Common Mistakes
 * ============================================================ */
void section6_common_mistakes(void)
{
    printf("=================================================\n");
    printf(" SECTION 6: Common Pointer Mistakes\n");
    printf("=================================================\n\n");

    /* MISTAKE 1: Uninitialized pointer (dangling/wild pointer) */
    printf("  MISTAKE 1: Wild pointer\n");
    printf("  int *p;        ← p points to GARBAGE address\n");
    printf("  *p = 5;        ← CRASH or silent corruption!\n");
    printf("  FIX: int *p = NULL;  always initialize!\n\n");

    /* MISTAKE 2: Dereferencing NULL */
    printf("  MISTAKE 2: NULL dereference\n");
    printf("  int *p = NULL;\n");
    printf("  *p = 5;        ← SEGFAULT!\n");
    printf("  FIX: if (p != NULL) { *p = 5; }\n\n");

    /* MISTAKE 3: Off-by-one */
    printf("  MISTAKE 3: Off-by-one\n");
    int arr[3] = {1, 2, 3};
    int *p = arr;
    printf("  int arr[3]; p = arr;\n");
    printf("  *(p+3) = ???  ← reading BEYOND array! undefined behavior\n");
    printf("  FIX: only access p+0, p+1, p+2\n\n");
    (void)p;

    /* MISTAKE 4: Returning pointer to local variable */
    printf("  MISTAKE 4: Returning local variable address\n");
    printf("  int* bad() {\n");
    printf("      int x = 5;    ← x lives on the stack\n");
    printf("      return &x;    ← stack destroyed after return!\n");
    printf("  }                 ← caller gets garbage\n");
    printf("  FIX: use static, malloc, or pass buffer in\n\n");

    /* MISTAKE 5: Pointer type mismatch */
    printf("  MISTAKE 5: Wrong pointer type\n");
    printf("  int  x = 0x12345678;\n");
    printf("  char *p = (char*)&x;\n");
    printf("  *p reads only 1 byte, not 4!\n\n");
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   C Pointers — Complete Walkthrough  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    section1_what_is_a_pointer();
    section2_pointer_types();
    section3_pointer_arithmetic();
    section4_arrays_and_pointers();
    section5_const_pointers();
    section6_common_mistakes();

    printf("=================================================\n");
    printf(" NEXT: 02_pointers_structs/\n");
    printf(" → pointers inside structs (the core of firmware)\n");
    printf("=================================================\n\n");

    return 0;
}
