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
#include "shell.h"
#include "kernel.h"
#include "stdbool.h"
#include "uart0.h"
#include "asp.h"
#include "mm.h"

// REQUIRED: Add header files here for your strings functions, ...

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add processing for the shell commands through the UART here
void shell(void)
{
    COMMAND_DATA data;

    // On startup clear putty screen and place cursors top-left
    putsUart0(CLEAR_PUTTY);
    putsUart0(HOME_POS);
    // Print in green "user@: " and go back to normal
    putsUart0(GREEN_TXT);
    putsUart0("user@: ");
    putsUart0(DFLT_TXT);


    while(true) {
        if(kbhitUart0()) {

            bool valid = false;
            getsUart0(&data);
            parseFields(&data);

            // Reboots the microcontroller. This will be implemented as part of the mini project
            if(isCommand(&data, "reboot", 0)) {
                putsUart0(BLUE_TXT);
                putsUart0("user@: ");
                reboot();
                valid = true;
            }

            //Displays the process (thread) status.
            else if(isCommand(&data, "ps", 0)) {
                ps();
                valid = true;
            }

            // Displays the inter-process (thread) communication status.
            else if(isCommand(&data, "ipcs", 0)) {
                ipcs();
                valid = true;
            }

            // Kills the process (thread) with the matching PID
            else if(isCommand(&data, "kill", 1)) {
                uint32_t pid = getFieldInteger(&data, 1);
                kill(pid);
                valid = true;
            }

            // Kills the thread based on the process name
            else if(isCommand(&data, "pkill", 1)) {
                char *name = getFieldString(&data, 1);
                pkill(name);
                valid = true;
            }

            //Turns priority inheritance ON or OFF.
            else if(isCommand(&data, "pi", 1)) {
                char *str1 = getFieldString(&data, 1);

                if(strgcmp(str1, "ON")) {
                    pi(true);
                    valid = true;
                }
                else if(strgcmp(str1, "OFF")) {
                    pi(false);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            //Turns preemption on or off.
            else if(isCommand(&data, "preempt", 1)) {
                char *str1 = getFieldString(&data, 1);

                if(strgcmp(str1, "ON")) {
                    preempt(true);
                    valid = true;
                }
                else if(strgcmp(str1, "OFF")) {
                    preempt(false);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            //Selected priority or round-robin scheduling. PRIO|RR
            else if(isCommand(&data, "sched", 1)) {
                char *str1 = getFieldString(&data, 1);

                if(strgcmp(str1, "PRIO")) {
                    sched(true);
                    valid = true;
                }
                else if(strgcmp(str1, "RR")) {
                    sched(false);
                    valid = true;
                }
                else {
                    valid = false;
                }
            }

            // Displays the PID of the process (thread).
            else if(isCommand(&data, "pidof", 1)) {
                char *name = getFieldString(&data, 1);
                pidof(name);
                valid = true;
            }

            //meminfo
            else if(isCommand(&data, "meminfo", 0)) {
                meminfo();
                valid = true;
            }

            // clears putty & places cursor at top
            else if(isCommand(&data, "clear", 0)) {
                putsUart0(CLEAR_PUTTY);
                putsUart0(HOME_POS);
                valid = true;
            }

            // User input new line
            else if(data.buffer[0] == 0) valid = true;

            // Runs the selected program in the background.
            else {
                char *name = getFieldString(&data, 0);
                valid = runProc(name);
            }

            if(!valid) {
                putsUart0(RED_TXT);
                putsUart0("Invalid Command\n\n");
            }

            // Print in green "user@: " and go back to normal
            putsUart0(GREEN_TXT);
            putsUart0("user@: ");
            putsUart0(DFLT_TXT);
        }

        yield();
    }
}

void reboot() {
    __asm(" SVC #8 ");
}

void ps() {
    PS ps[MAX_TASKS] = {0};
    __asm(" SVC #9 ");

    uint8_t i = 0;
    putsUart0("\nProcess\t\tPID#\t %CPU\t  State       S    M\n");
    putsUart0("------------------------------------------------------\n");
    while(ps[i].PID) {
        printPS(ps[i].name, ps[i].PID, ps[i].cpu, ps[i].state, ps[i].sem, ps[i].mtx);
        i++;
    }
    putsUart0("------------------------------------------------------\n\n");
}

void ipcs() {
    IPCS ipcs[MAX_SEMAPHORES+MAX_MUTEXES] = {0};
    __asm(" SVC #10 ");

    uint8_t i, j;

    putsUart0("\nSemaph\t\tCount\t\tQ-Size\tQueue\n");
    putsUart0("--------------------------------------------------\n");
    for(i=0; i<MAX_SEMAPHORES; i++)
        printSem(i, ipcs[i].sCount, ipcs[i].qSize, ipcs[i].q);
    putsUart0("--------------------------------------------------\n\n");

    putsUart0("\nMutex\tState\tLock-By\t\tQ-Size\tQueue\n");
    putsUart0("--------------------------------------------------\n");
    for(j=0; j<MAX_MUTEXES; j++)
        printMtx(j, ipcs[i+j].mLock, ipcs[i+j].mLockedBy, ipcs[i+j].qSize, ipcs[i+j].q);
    putsUart0("--------------------------------------------------\n\n");

}

void kill(uint32_t pid) {
    __asm(" SVC #11 ");

    uint8_t good = getR0();

    if(good == SUCCESS) {
        putsPidKilled(pid);
    }
    else if(good == FAILURE) {
        putsUart0(RED_TXT);
        putsUart0("Error: PID not found\n\n");
    }
    else {
        putsUart0(RED_TXT);
        putsUart0("Process has already been terminated!\n\n");
    }
}

void pkill(char *name) {
    __asm(" SVC #12 ");

    uint8_t good = getR0();

    if(good == SUCCESS) {
        putsUart0(name);
        putsUart0(" killed\n\n");
    }
    else if(good == FAILURE){
        putsUart0(RED_TXT);
        putsUart0("Error: process name not found\n\n");
    }
    else {
        putsUart0(RED_TXT);
        putsUart0("Process has already been terminated!\n\n");
    }
}

void pi(bool isEnable) {
    __asm(" SVC #13 ");

    if(isEnable)
        putsUart0("pi on\n\n");
    else
        putsUart0("pi off\n\n");
}

void preempt(bool isEnable) {
    __asm(" SVC #14 ");

    if(isEnable)
        putsUart0("preempt on\n\n");
    else
        putsUart0("preempt off\n\n");
}

void sched(bool prioEnable) {
    __asm(" SVC #15 ");

    if(prioEnable)
        putsUart0("sched prio\n\n");
    else
        putsUart0("sched rr\n\n");
}

void pidof(const char name[]) {
    __asm(" SVC #16 ");

    uint32_t pid = getR0();

    if(pid) {
        display("PID: 0x", pid, 1, 4);
        putsUart0("\n");
    }
    else {
        putsUart0(RED_TXT);
        putsUart0("Error: process name not found\n\n");
    }
}

bool runProc(char *name) {
    __asm(" SVC #17 ");

    uint8_t good = getR0();

    if(good == SUCCESS) {
        putsUart0("Running ");
        putsUart0(name);
        putsUart0(" in the background... \n\n");
        return true;
    }
    else if(good == FAILURE) {
        return false;
    }
    else
        putsUart0("Process is currently running...\n\n");

    return true;
}

void meminfo() {
    MEM mem[HCB_MAX_SIZE] = {0};
    __asm(" SVC #18 ");

    uint8_t i = 0;
    uint32_t total = 0;
    uint32_t free = 0;

    putsUart0("\nPID\t\tBase Address\tAlloc-Size\n");
    putsUart0("------------------------------------------\n");
    while(mem[i].size) {
        printMem(mem[i].pid, mem[i].baseAdd, mem[i].size);
        total += mem[i].size;
        i++;
    }
    putsUart0("------------------------------------------\n");
    display("Memory Used:\t\t\t", total, 0, 0);
    putsUart0("------------------------------------------\n");
    display("Total Memory:\t\t\t", MEM_TOTAL, 0, 0);
    free = MEM_TOTAL - total;
    display("Memory Free:\t\t\t", free, 0, 0);
    putsUart0("------------------------------------------\n\n");
}

