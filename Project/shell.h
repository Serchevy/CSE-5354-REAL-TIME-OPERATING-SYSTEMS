// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

#include <stdbool.h>

typedef struct _PS {
    char name[16];
    uint32_t PID;
    uint16_t cpu;
    uint8_t state;
    uint8_t sem;
    uint8_t mtx;
} PS;

typedef struct _IPCS {
    bool mLock;
    uint8_t sCount;
    uint8_t qSize;
    uint32_t q[2];
    uint32_t mLockedBy;
} IPCS;

typedef struct _MEM {
    uint32_t pid;
    uint32_t baseAdd;
    uint16_t size;
} MEM;

#define MEM_TOTAL 0x7000

#define SUCCESS     1
#define FAILURE     0

#define HOME_POS        "\033[H"
#define SAVE_POS        "\033[s"
#define RESTORE_POS     "\033[u"
#define CLEAR_PUTTY     "\033[2J"
#define CLEAR_LINE      "\033[2K"
#define HIDE_CURSOR     "\033[?25l"
#define SHOW_CURSOR     "\033[?25h"

#define DFLT_TXT        "\033[0m"
#define BLACK_TXT       "\033[30m"
#define RED_TXT         "\033[31m"
#define GREEN_TXT       "\033[32m"
#define YELLOW_TXT      "\033[33m"
#define BLUE_TXT        "\033[34m"
#define MAGENTA_TXT     "\033[35m"
#define CYAN_TXT        "\033[36m"
#define WHITE_TXT       "\033[37m"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void shell();

void reboot();
void ps();
void ipcs();
void kill(uint32_t pid);
void pkill(char *name);
void pi(bool on);
void preempt(bool on);
void sched(bool prio_on);
void pidof(const char name[]);
bool runProc(char *name);
void meminfo();

#endif
