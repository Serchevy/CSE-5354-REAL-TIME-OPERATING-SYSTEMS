
//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef SHELL_H_
#define SHELL_H_

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void reboot();
void ps();
void ipcs();
void kill(uint32_t pid);
void pi(bool on);
void preempt(bool on);
void sched(bool prio_on);
void pidof(const char name[]);
void shell(void);

void yield();

#endif
