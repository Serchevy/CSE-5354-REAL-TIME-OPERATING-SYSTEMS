//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "mpu.h"
#include "mem.h"
#include "tm4c123gh6pm.h"s

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initMPU() {
    allowFlashAccess();         // 0x0000.0000 - 0x0003.FFFF
    allowPeripheralAccess();    // 0x4000.0000 - 0x43FF.FFFF
    setupSramAccess();          // 0x2000.0000 - 0x2000.7FFF

    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_PRIVDEFEN | NVIC_MPU_CTRL_ENABLE;
}

void allowFlashAccess(void) {                                       // Flash
    NVIC_MPU_NUMBER_R = 0x5;
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & FLASH_BASE;           // N = 18, add is bits 31-18
                                                                    // size = N-1 => 17
    //          Exec enb | RW priv & unpriv | flash | size | enable region
    NVIC_MPU_ATTR_R |= 0x0<<28 | 0x3<<24 | 0x2<<16 | 0x11<<1 | 0x1;
}

void allowPeripheralAccess(void) {                                  // Peripherals
    NVIC_MPU_NUMBER_R = 0x6;
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & PERIP_BASE;           // N = 26, base_add bits 31-26
                                                                    // size = N-1 => 25
    //          Exec dis | RW priv & unpriv | periph | size | enable region
    NVIC_MPU_ATTR_R |= 0x0<<28 | 0x3<<24 | 0x5<<16 | 0x19<<1 | 0x1;
}

void setupSramAccess(void) {                                                    // SRAM

    NVIC_MPU_NUMBER_R = 0x0;                                                    // R0 - 4k
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & R0_4k;                            // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;     // Exec dis | Full Access | sram | size | enable region

    NVIC_MPU_NUMBER_R = 0x1;                                                   // R1 - 8k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R1_8k;                             // N = 13 & size = 12
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xC<<1 | 0x1;     // Exec dis | Full Access | sram | size | enable region

    NVIC_MPU_NUMBER_R = 0x2;                                                   // R2 - 4k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R2_4k;                             // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;     // Exec dis | Full Access | sram | size | enable region

    NVIC_MPU_NUMBER_R = 0x3;                                                    // R3 - 4k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R3_4k;                             // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;     // Exec dis | Full Access | sram | size | enable region

    NVIC_MPU_NUMBER_R = 0x4;                                                    // R4 - 8k
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & R4_8k;                            // N = 13 & size = 12
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xC<<1 | 0x1;     // Exec dis | Full Access | sram | size | enable region
}

uint64_t createNoSramAccessMask(void) {
    return 0xFFFFFFFFFFFFFFFF;
}

void applySramAccessMask(uint64_t srdBitMask) {
    uint8_t region_srdBitMask[5] = {0};
    uint8_t i;

    for(i=0; i<5; i++)
        region_srdBitMask[i] = (srdBitMask >> i*8) & 0xFF;      // Extract bits into the 5 MPU regions

    for(i=0; i<5; i++) {
        NVIC_MPU_NUMBER_R = i;
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M;
        NVIC_MPU_ATTR_R |= (region_srdBitMask[i]<<8);           // Apply srd bits to respective region
    }
}

void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes) {
    uint8_t i = 0;
    uint8_t start = find_SR(baseAdd);
    uint8_t end = start;
    uint16_t subreg_size = 0;
    uint16_t added_size = 0;

    while(added_size < size_in_bytes) {
        if((end >= 8 && end <= 15) || (end >=32 && end <=39)) subreg_size = 1024;
        else subreg_size = 512;

        added_size += subreg_size;
        end++;
    }

    for(i=0; i<(end-start); i++)
        *srdBitMask &= ~(1ULL << (start + i));

    applySramAccessMask(*srdBitMask);
}
