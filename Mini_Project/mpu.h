//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef MPU_H_
#define MPU_H_

#include <stdint.h>
#include <stdbool.h>

#define FLASH_BASE  0x00000000
#define PERIP_BASE  0x40000000
#define SRAM_BASE   0x20000000

// Memory Layout
#define R0_4k       0x20001000
#define R1_8k       0x20002000
#define R2_4k       0x20004000
#define R3_4k       0x20005000
#define R4_8k       0x20006000

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initMPU();

void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void applySramAccessMask(uint64_t srdBitMask);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes);

#endif
