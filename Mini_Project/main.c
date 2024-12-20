//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <mem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "clock.h"
#include "uart0.h"
#include "main.h"
#include "asp.h"
#include "shell.h"
#include "mem.h"
#include "mpu.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void usageFault() {
    uint32_t x = 0;
    uint32_t y = 10/x;
}

void busFault() {
    setPSP(TOP_SRAM);
}

void hardFault() {
    __asm(" MOV R0, #0xDEAD");
    __asm(" MOV R1, #0xBEEF");
    __asm(" MOV R2, #0XFEED");
    __asm(" MOV R3, #0xAAAA");
    __asm(" MOV R12, #0xBBBB");

    NVIC_CFG_CTRL_R &= ~NVIC_CFG_CTRL_DIV0;
    NVIC_SYS_HND_CTRL_R &= ~(NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM);
    setPSP(TOP_SRAM);
}

void mpuFault() {
    __asm(" MOV R0, #0xDEAD");
    __asm(" MOV R1, #0xBEEF");
    __asm(" MOV R2, #0XFEED");
    __asm(" MOV R3, #0xAAAA");
    __asm(" MOV R12, #0xBBBB");

    setTMPL();                                      // Go into upriv mode
    uint32_t *ptr = (uint32_t *)0x20000200;
    *ptr = 0xDEAD;                                  // Attempt a write to OS
}

void pendsv() {
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

void sysTick() {
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PENDSTSET;
}

void initHw() {

    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6);

    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM;

    enablePort(PORTA);
    enablePort(PORTC);
    enablePort(PORTD);
    enablePort(PORTE);
    enablePort(PORTF);

    selectPinPushPullOutput(RED_LED_uC);

    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(ORANGE_LED);
    selectPinPushPullOutput(YELLOW_LED);
    selectPinPushPullOutput(GREEN_LED);

    selectPinDigitalInput(PB0);
    enablePinPullup(PB0);
    selectPinDigitalInput(PB1);
    enablePinPullup(PB1);
    selectPinDigitalInput(PB2);
    enablePinPullup(PB2);
    selectPinDigitalInput(PB3);
    enablePinPullup(PB3);
    selectPinDigitalInput(PB4);
    enablePinPullup(PB4);
    setPinCommitControl(PB5);
    selectPinDigitalInput(PB5);
    enablePinPullup(PB5);
}

void test() {
    uint64_t access = createNoSramAccessMask();
    uint32_t *p = malloc_from_heap(1024);

    addSramAccessWindow(&access, p, 0x400);
    addSramAccessWindow(&access, (void*)0x20007C00, 0x200);        // Give SP acces

//    setPSP(TOP_SRAM - 0x1000);
//    setASP();
    setTMPL();

    *p = 0xFEED;

    setPinValue(BLUE_LED, 1);
}

void test_malloc() {
    uint64_t access = createNoSramAccessMask();
    addSramAccessWindow(&access, 0x20001000, 0x7000);

    void *alloc_A = malloc_from_heap(512);
    void *alloc_B = malloc_from_heap(1024);
    void *alloc_C = malloc_from_heap(512);
    void *alloc_D = malloc_from_heap(1536);
    void *alloc_E = malloc_from_heap(1024);
    void *alloc_F = malloc_from_heap(1024);
    void *alloc_G = malloc_from_heap(1024);
    void *alloc_H = malloc_from_heap(1024);
    void *alloc_I = malloc_from_heap(512);
    void *alloc_J = malloc_from_heap(4096);

    void *alloc_K = malloc_from_heap(5000);

    free_to_heap(alloc_J);
//    void *alloc_K = malloc_from_heap(3000);
//    free_to_heap(alloc_D);
//    void *alloc_M= malloc_from_heap(1500);

    setTMPL();
}


int main(void) {

    // RTOS
    setPSP(TOP_SRAM);
    setASP();
    initHw();
    initMPU();

    //setTMPL();
    //test();
    test_malloc();

    shell();
}
