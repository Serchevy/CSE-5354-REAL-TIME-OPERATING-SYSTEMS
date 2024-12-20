//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef MEM_H_
#define MEM_H_

#include <stdint.h>
#include <stdbool.h>

#define BASE_ADD 0x20001000

typedef struct _HCB {
    bool inUse;
    void *ptr;
    uint16_t size;      // Max size 8kiB
    uint32_t PID;
} HCB;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initMPU();

bool subregs_free(int8_t idx, uint8_t subregs_to_use);
void *getAddress(int8_t SR);
int8_t find_SR(void *ptr);
void *malloc_from_heap(uint32_t size_in_bytes);
void free_to_heap(void *ptr);

#endif
