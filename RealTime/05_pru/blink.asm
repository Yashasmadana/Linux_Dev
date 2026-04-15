; ============================================================
; 05_pru/blink.asm
;
; TOPIC: PRU Firmware — Blink GPIO at exact frequency
;
; This runs ON the PRU core (not Linux/ARM side)
; Every instruction = exactly 5ns at 200MHz
;
; Compile:  pasm -b blink.asm
; Output:   blink.out  → copy to /lib/firmware/blink_pru0.out
;
; Hardware: LED on P8_11 = PRU0 GPIO bit 15
;
; Shared memory layout (must match blink.c):
;   Offset 0x00: command   (0=stop, 1=blink)
;   Offset 0x04: period_ms
;   Offset 0x08: count     (blink counter, written by PRU)
;   Offset 0x0C: status    (0=idle, 1=running)
; ============================================================

.origin 0
.entrypoint START

; Register aliases
#define SHARED_RAM  C28         ; PRU shared RAM constant table entry
#define GPIO_BASE   0x44E07000  ; GPIO0 base address
#define GPIO_SETDATAOUT   0x194 ; Set GPIO high
#define GPIO_CLEARDATAOUT 0x190 ; Set GPIO low
#define P8_11_BIT   (1 << 13)  ; GPIO0[13] = P8_11

; Shared memory offsets
#define OFF_CMD     0
#define OFF_PERIOD  4
#define OFF_COUNT   8
#define OFF_STATUS  12

START:
    ; Signal to Linux we are running
    MOV  r1, 1
    SBCO r1, SHARED_RAM, OFF_STATUS, 4

MAIN_LOOP:
    ; Read command from shared memory
    LBCO r0, SHARED_RAM, OFF_CMD, 4

    ; Check command
    QBEQ BLINK,     r0, 1       ; if cmd==1 → blink
    QBEQ LED_ON,    r0, 2       ; if cmd==2 → solid on
    QBEQ LED_OFF,   r0, 3       ; if cmd==3 → solid off

    ; cmd==0: stop — turn off LED and signal idle
    JMP  STOP

; ----------------------------------------
; BLINK: toggle LED based on period_ms
; ----------------------------------------
BLINK:
    ; Read period from shared memory
    LBCO r2, SHARED_RAM, OFF_PERIOD, 4

    ; Convert ms to PRU cycles (200000 cycles per ms)
    MOV  r3, 200000
    MUL  r2, r2, r3             ; r2 = half-period in cycles

    ; Turn LED ON
    MOV  r4, GPIO_BASE
    MOV  r5, P8_11_BIT
    SBBO r5, r4, GPIO_SETDATAOUT, 4

    ; Wait half period
    MOV  r6, r2
DELAY_ON:
    SUB  r6, r6, 1
    QBNE DELAY_ON, r6, 0

    ; Turn LED OFF
    SBBO r5, r4, GPIO_CLEARDATAOUT, 4

    ; Wait half period
    MOV  r6, r2
DELAY_OFF:
    SUB  r6, r6, 1
    QBNE DELAY_OFF, r6, 0

    ; Increment blink count
    LBCO r7, SHARED_RAM, OFF_COUNT, 4
    ADD  r7, r7, 1
    SBCO r7, SHARED_RAM, OFF_COUNT, 4

    ; Loop back
    JMP  MAIN_LOOP

; ----------------------------------------
; LED_ON / LED_OFF
; ----------------------------------------
LED_ON:
    MOV  r4, GPIO_BASE
    MOV  r5, P8_11_BIT
    SBBO r5, r4, GPIO_SETDATAOUT, 4
    JMP  MAIN_LOOP

LED_OFF:
    MOV  r4, GPIO_BASE
    MOV  r5, P8_11_BIT
    SBBO r5, r4, GPIO_CLEARDATAOUT, 4
    JMP  MAIN_LOOP

; ----------------------------------------
; STOP
; ----------------------------------------
STOP:
    ; Turn off LED
    MOV  r4, GPIO_BASE
    MOV  r5, P8_11_BIT
    SBBO r5, r4, GPIO_CLEARDATAOUT, 4

    ; Signal idle
    MOV  r1, 0
    SBCO r1, SHARED_RAM, OFF_STATUS, 4

    ; Halt PRU
    HALT
