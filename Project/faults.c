// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "faults.h"
#include "uart0.h"
#include "asp.h"
#include "shell.h" 
#include "kernel.h"


extern uint8_t taskCurrent;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: If these were written in assembly
//           omit this file and add a faults.s file

// REQUIRED: code this function
void mpuFaultIsr(void)
{
    putsUart0(RED_TXT);
    display("\nMPU Fault in Process 0x", (uint32_t)getPID(), 1, 4);
    putsUart0(DFLT_TXT); 
    putsUart0("------------------\n");

    // Provide the value of the PSP, MSP, and mfault flags (in hex).
    uint32_t *PSP = getPSP();
    uint32_t *MSP = getMSP();
    uint32_t mfault = NVIC_FAULT_STAT_R & 0xFF;

    display("PSP:    0x", (uint32_t)PSP, 1, 8);
    display("MSP:    0x", (uint32_t)MSP, 1, 8);
    display("mflags: 0x", mfault, 1, 8);

    // Also, print the offending instruction and data addresses.
    uint16_t instr = *( (uint32_t *)(*(PSP+6)));
    uint32_t memAdr = NVIC_MM_ADDR_R;

    putsUart0("------------------\n");
    display("ofsIns: 0x", instr, 1, 8);
    display("memAdr: 0x", memAdr, 1, 8);
    putsUart0("------------------\n");

    // Display the process stack dump (xPSR, PC, LR, R0-3, R12.
    stackDump(PSP);

    // Kill thread that caused the fault
    taskKill(taskCurrent);

    // Clear the MPU fault pending bit and trigger a pendsv ISR call
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_MEMP;
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

// REQUIRED: code this function
void hardFaultIsr(void)
{
    putsUart0(RED_TXT);
    display("Hard Fault in Process 0x", (uint32_t)getPID(), 1, 4);
    putsUart0(DFLT_TXT); 
    putsUart0("------------------\n");

    // Provide the value of the PSP, MSP & mfault flags (in hex)
    uint32_t *PSP = getPSP();
    uint32_t *MSP = getMSP();
    uint32_t mfault = NVIC_FAULT_STAT_R & 0xFF; // only mem flags

    display("PSP:    0x", (uint32_t)PSP, 1, 8);
    display("MSP:    0x", (uint32_t)MSP, 1, 8);
    display("mflags: 0x", mfault, 1, 8);

    // print the offending instruction.
    uint16_t instr = *( (uint32_t *)(*(PSP+6)));

    putsUart0("------------------\n");
    display("ofsIns: 0x", instr, 1, 8);
    putsUart0("------------------\n");

    // Display the process stack dump (xPSR, PC, LR, R0-3, R12.
    stackDump(PSP);
    while(1);
}

// REQUIRED: code this function
void busFaultIsr(void)
{
    putsUart0(RED_TXT);
    display("\nBus Fault in Process 0x", (uint32_t)getPID(), 1, 4);
    putsUart0(DFLT_TXT); 
    while(1);
}

// REQUIRED: code this function
void usageFaultIsr(void)
{
    putsUart0(RED_TXT);
    display("\nUsage Fault in Process 0x", (uint32_t)getPID(), 1, 4);
    putsUart0(DFLT_TXT);  
    while(1);
}

void stackDump(uint32_t *PSP)
{
    uint32_t R0 = *PSP;
    uint32_t R1 = *(PSP+1);
    uint32_t R2 = *(PSP+2);
    uint32_t R3 = *(PSP+3);
    uint32_t R12 = *(PSP+4);
    uint32_t LR = *(PSP+5);
    uint32_t PC = *(PSP+6);
    uint32_t xPSR = *(PSP+7);

    putsUart0("    STACK DUMP\n");
    display(" R0:   0x", R0, 1, 8);
    display(" R1:   0x", R1, 1, 8);
    display(" R2:   0x", R2, 1, 8);
    display(" R3:   0x", R3, 1, 8);
    display(" R12:  0x", R12, 1, 8);
    display(" LR:   0x", LR, 1, 8);
    display(" PC:   0x", PC, 1, 8);
    display(" xPSR: 0x", xPSR, 1, 8);
    putsUart0("------------------\n\n");
}

