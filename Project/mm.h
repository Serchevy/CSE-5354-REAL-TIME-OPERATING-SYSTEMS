// Memory manager functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef MM_H_
#define MM_H_

#include <stdbool.h>

#define HCB_MAX_SIZE 19

typedef struct _HCB {
    void *ptr;
    void *SP;
    uint16_t size;      // Max size 8kiB
    void* PID;
} HCB;
HCB HCB_table[HCB_MAX_SIZE+1] = {};

#define FLASH_BASE  0x00000000          // 0x0000.0000 - 0x0003.FFFF    2^18
#define PERIP_BASE  0x40000000          // 0x4000.0000 - 0x43FF.FFFF    2^26
#define SRAM_BASE   0x20000000          // 0x2000.1000 - 0x2000.7FFF
#define BASE_ADD    0x20001000

// Memory Layout
#define R0_4k       0x20001000
#define R1_8k       0x20002000
#define R2_4k       0x20004000
#define R3_4k       0x20005000
#define R4_8k       0x20006000

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool subregs_free(int8_t idx, uint8_t subregs_to_use);
void *getAddress(int8_t SR);
int8_t find_SR(void *ptr);

void *mallocFromHeap(uint32_t size_in_bytes);
void freeToHeap(void *pMemory);
void freeTask(uint8_t task);

void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes);
void applySramAccessMask(uint64_t srdBitMask);

#endif
