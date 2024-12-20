//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "shell.h"
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "clock.h"
#include "uart0.h"
#include "main.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void ps() {
    putsUart0("PS called\n");
}

void reboot() {
    NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
}

void ipcs() {
    putsUart0("IPCS called\n");
}

void kill(uint32_t pid) {
    char str[100];
    itos(pid, str, 0);
    putsUart0(str);
    putsUart0(" killed\n");
}

void pi(bool on) {
    if(on) {
        putsUart0("pi on\n");
    }
    else {
        putsUart0("pi off\n");
    }
}

void preempt(bool on) {
    if(on) {
        putsUart0("preempt on\n");
    }
    else {
        putsUart0("preempt off\n");
    }
}

void sched(bool prio_on) {
    if(prio_on) {
        putsUart0("sched prio\n");
    }
    else {
        putsUart0("sched rr\n");
    }
}

void pidof(const char name[]) {

    putsUart0(name);
    putsUart0(" launched\n");
}

void yield() {

}

void shell(void) {
    COMMAND_DATA data;
    char arr[][10] = {"idle", "flash4hz", "run", "stop"};

    while(true) {
        if(kbhitUart0()) {

            bool valid = false;
            getsUart0(&data);
            parseFields(&data);

            // Reboots the microcontroller. This will be implemented as part of the mini project
            if(isCommand(&data, "reboot", 0)) {
                putsUart0("Rebooting...\n");
                reboot();
                valid = true;
            }

            //Displays the process (thread) status.
            if(isCommand(&data, "ps", 0)) {
                ps();
                valid = true;
            }

            // Displays the inter-process (thread) communication status.
            if(isCommand(&data, "ipcs", 0)) {
                ipcs();
                valid = true;
            }

            // Kills the process (thread) with the matching PID
            if(isCommand(&data, "kill", 1)) {
                uint32_t pid = getFieldInteger(&data, 1);
                kill(pid);
                valid = true;
            }

            // Kills the thread based on the process name
            if(isCommand(&data, "pkill", 1)) {
                char *name = getFieldString(&data, 1);
                putsUart0(name);
                putsUart0(" killed\n");
                valid = true;
            }

            //Turns priority inheritance ON or OFF.
            if(isCommand(&data, "pi", 1)) {
                char *str1 = getFieldString(&data, 1);
                bool on;

                if(strgcmp(str1, "ON")) {
                    on = true;
                    pi(on);
                    valid = true;
                }
                else if(strgcmp(str1, "OFF")) {
                    on = false;
                    pi(on);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            //Turns preemption on or off.
            if(isCommand(&data, "preempt", 1)) {
                char *str1 = getFieldString(&data, 1);
                bool on;

                if(strgcmp(str1, "ON")) {
                    on = true;
                    preempt(on);
                    valid = true;
                }
                else if(strgcmp(str1, "OFF")) {
                    on = false;
                    preempt(on);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            //Selected priority or round-robin scheduling. PRIO|RR
            if(isCommand(&data, "sched", 1)) {
                char *str1 = getFieldString(&data, 1);
                bool pri_on;

                if(strgcmp(str1, "PRIO")) {
                    pri_on = true;
                    sched(pri_on);
                    valid = true;
                }
                else if(strgcmp(str1, "RR")) {
                    pri_on = false;
                    sched(pri_on);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            // Displays the PID of the process (thread).
            if(isCommand(&data, "pidof", 1)) {
                char *name = getFieldString(&data, 1);
                pidof(name);
                valid = true;
            }

            // Runs the selected program in the background. For now, turn on the red LED.
            uint8_t i = 0;
            for(i = 0; i<4; i++) {
                if(isCommand(&data, arr[i], 0)) {
                    char *name = getFieldString(&data, 0);
                    putsUart0("Running ");
                    putsUart0(name);
                    putsUart0("\n");
                    togglePinValue(RED_LED_uC);
                    valid = true;
                }
            }

            if (!valid) {
                putsUart0("Invalid command\n");
            }

        }

        if(!getPinValue(PB0))
            usageFault();
        else if(!getPinValue(PB1))
            busFault();
        else if(!getPinValue(PB2))
            hardFault();
        else if(!getPinValue(PB3))
            mpuFault();
        else if(!getPinValue(PB4))
            pendsv();
        else if(!getPinValue(PB5))
            sysTick();

        yield();

    }
}
