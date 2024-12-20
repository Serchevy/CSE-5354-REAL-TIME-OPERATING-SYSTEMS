//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include <stdbool.h>

#define TOP_SRAM    0x20008000

#define RED_LED_uC  PORTF,1

#define BLUE_LED    PORTF,2
#define RED_LED     PORTA,2
#define ORANGE_LED  PORTA,3
#define YELLOW_LED  PORTA,4
#define GREEN_LED   PORTE,0

#define PB0         PORTC,4
#define PB1         PORTC,5
#define PB2         PORTC,6
#define PB3         PORTC,7
#define PB4         PORTD,6
#define PB5         PORTD,7

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void usageFault();
void busFault();
void hardFault();
void mpuFault();
void pendsv();
void sysTick();

void initHw();

#endif
