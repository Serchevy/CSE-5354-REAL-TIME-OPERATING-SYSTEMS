// Tasks
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
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "wait.h"
#include "kernel.h"
#include "tasks.h"
#include "mm.h"

#define BLUE_LED    PORTF,2 // on-board blue LED
#define RED_LED     PORTA,2 // off-board red LED
#define ORANGE_LED  PORTA,3 // off-board orange LED
#define YELLOW_LED  PORTA,4 // off-board yellow LED
#define GREEN_LED   PORTE,0 // off-board green LED

#define PB0         PORTC,4
#define PB1         PORTC,5
#define PB2         PORTC,6
#define PB3         PORTC,7
#define PB4         PORTD,6
#define PB5         PORTD,7

#define TEST_LED    PORTF,3 // on-board green LED

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
//           Add initialization for 6 pushbuttons
void initHw(void)
{
    // Enable Faults
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM;

    // Setup LEDs and pushbuttons
    enablePort(PORTA);
    enablePort(PORTC);
    enablePort(PORTD);
    enablePort(PORTE);
    enablePort(PORTF);

    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(ORANGE_LED);
    selectPinPushPullOutput(YELLOW_LED);
    selectPinPushPullOutput(GREEN_LED);

    selectPinPushPullOutput(TEST_LED);
    setPinAuxFunction(TEST_LED, 5);

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

    // Power-up flash
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(250000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(250000);

    // Enalbe MPU
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_PRIVDEFEN | NVIC_MPU_CTRL_ENABLE;

    // Configure a free running timer to time tasks
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R0;
    _delay_cycles(3);
    WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;
    WTIMER0_TAMR_R |= TIMER_TAMR_TAMR_1_SHOT | TIMER_TAMR_TACDIR;
    WTIMER0_TAV_R = 0;

    initLEDpwm();
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void initLEDpwm() {
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R1;
    _delay_cycles(3);

    // GREEN on M1PWM7 (PF3), M1PWM3b
    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R1;               // reset PWM1 module
    SYSCTL_SRPWM_R = 0;                             // leave reset state
    PWM1_3_CTL_R = 0;                               // turn-off PWM1 generator 3 (drives outs 6 and 7)
    PWM1_3_GENB_R = PWM_1_GENB_ACTCMPBD_ONE | PWM_1_GENB_ACTLOAD_ZERO;
                                                    // output 7 on PWM1, gen 3b, cmpb
    PWM1_3_LOAD_R = 1024;
    PWM1_3_CMPB_R = 0;                              // Green LED off

    PWM1_3_CTL_R = PWM_1_CTL_ENABLE;                // turn-on PWM1 generator 3
    PWM1_ENABLE_R = PWM_ENABLE_PWM7EN;              // Enalbe output
}

uint16_t dutyCycle = 0;
int8_t step = 1;

void wTimer1Isr() {
    if(dutyCycle > 100)
        dutyCycle = 100;

    PWM1_3_CMPB_R = 1023 - (dutyCycle * (1023 / 100));      // Set duty Cycle

    dutyCycle += step;                                      // increment or decrement duty Cycle

    if(dutyCycle >= 100)                                    // Account for upper and lower limits
        step = -1;
    else if(dutyCycle <= 5)
        step = 1;

    WTIMER1_ICR_R = TIMER_ICR_TATOCINT;     // Clear interrupt
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


// REQUIRED: add code to return a value from 0-63 indicating which of 6 PBs are pressed
uint8_t readPbs(void)
{
    uint8_t PBs = !getPinValue(PB5)<<5 | !getPinValue(PB4)<<4 | !getPinValue(PB3)<<3
                | !getPinValue(PB2)<<2 | !getPinValue(PB1)<<1 | !getPinValue(PB0)<<0;

    return PBs;
}

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle(void)
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}

void idle2(void)
{
    while(true)
    {
        setPinValue(GREEN_LED, 1);
        waitMicrosecond(1000);
        setPinValue(GREEN_LED, 0);
        yield();
    }
}

void flash4Hz(void)
{
    while(true)
    {
        setPinValue(GREEN_LED, !getPinValue(GREEN_LED));
        sleep(125);
    }
}

void oneshot(void)
{
    while(true)
    {
        wait(flashReq);
        setPinValue(YELLOW_LED, 1);
        sleep(1000);
        setPinValue(YELLOW_LED, 0);
    }
}

void partOfLengthyFn(void)
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn(void)
{
    uint16_t i;
    uint8_t *mem;
    mem = _mallocFromHeap(5000 * sizeof(uint8_t));
    while(true)
    {
        lock(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
            mem[i] = i % 256;
        }
        setPinValue(RED_LED, !getPinValue(RED_LED));
        unlock(resource);
    }
}

void readKeys(void)
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            setPinValue(YELLOW_LED, !getPinValue(YELLOW_LED));
            setPinValue(RED_LED, 1);
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            setPinValue(RED_LED, 0);
        }
        if ((buttons & 4) != 0)
        {
            restartThread(flash4Hz);
        }
        if ((buttons & 8) != 0)
        {
            stopThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce(void)
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative(void)
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant(void)
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important(void)
{
    while(true)
    {
        lock(resource);
        setPinValue(BLUE_LED, 1);
        sleep(1000);
        setPinValue(BLUE_LED, 0);
        unlock(resource);
    }
}
