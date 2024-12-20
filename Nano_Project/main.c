#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "clock.h"
#include "uart0.h"
#include "shell.h"

#define RED_LED     PORTF,1


// Global Variables
//----------------------------------------------------------------


//----------------------------------------------------------------

void initHw() {

    initSystemClockTo40Mhz();

    initUart0();
    setUart0BaudRate(115200, 40e6);

    enablePort(PORTF);
    selectPinPushPullOutput(RED_LED);
}

int main(void) {

    // Initialize hardware
    initHw();

    shell();
}
