// Memory manager functions
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
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "mm.h"
#include "kernel.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint64_t subRegInUse = 0;
uint8_t numAllocs = 0;

// REQUIRED: add your malloc code here and update the SRD bits for the current thread
void *mallocFromHeap(uint32_t size_in_bytes)
{
    if(size_in_bytes > 0x2000 || size_in_bytes == 0x0) return 0;       // 8kiB cap

    uint8_t i, j;
    int8_t best_idx = -1;
    uint16_t best_fit = 0x2000+1;
    uint8_t subregs_to_use = 0;

    if(size_in_bytes <= 0x200) {                                        // size <= 512 check sub-regs R0, R2, R3
        for(i=0; i<17; i+=16) {
            for(j=i; j<((i==0) ? 8 : 32); j++) {                        // 0-7 else 16-31
                if(!(subRegInUse & (1ULL << j))) {
                    if(0x200 < best_fit) {
                        best_fit = 0x200;
                        best_idx = j;
                    }
                }
            }
        }
        if(best_idx == -1) {                                            // No 512 available, use 1024
            for(i=8; i<33; i+=24) {
                for(j=i; j<8+i; j++) {
                    if(!(subRegInUse & (1ULL << j))) {
                        if(0x400 < best_fit) {
                            best_fit = 0x400;
                            best_idx = j;
                        }
                    }
                }
            }
        }
        subregs_to_use = 1;
    }
    else if(size_in_bytes > 0x200 && size_in_bytes <= 0x400 ) {         // size <= 1024 check sub-regs R1, R4
        for(i=8; i<33; i+=24) {
            for(j=i; j<8+i; j++) {
                if(!(subRegInUse & (1ULL << j))) {
                    if(0x400 < best_fit) {
                        best_fit = 0x400;
                        best_idx = j;
                    }
                }
            }
        }
        subregs_to_use = 1;
    }
    else if(size_in_bytes > 0x400 && size_in_bytes <= 0x600) {          // size <= 1536 Region edges
        for(i=7; i<32; i += (i == 7 ? 8 : 16)) {                        // go to indexes 7, 15, 31
            if(!(subRegInUse & (1ULL << i)) && !(subRegInUse & (1ULL << i+1))) {
                if(0x600 < best_fit) {
                    best_fit = 0x600;
                    best_idx = i;
                }
            }
        }
        subregs_to_use = 2;
        if(best_idx == -1) {                                                // if no region edged free
            subregs_to_use = 3;
            for(i=0; i<17; i+=16) {                                         // take three 512s
                for(j=i; j<((i==0) ? 8 : 32); j++) {
                    if(j+subregs_to_use-1 < i+((i==0) ? 8 : 32)) {          // stay within region
                        if(subregs_free(j, subregs_to_use) && 0x600 < best_fit) {
                            best_fit = 0x600;
                            best_idx = j;
                        }
                    }
                }
            }
        }
    }
    else if(size_in_bytes > 0x600 && size_in_bytes <= 0x1000) {         // size <= 4096 -> Regions R0, R2, R3
        subregs_to_use = (size_in_bytes + 512 - 1)/512;
        for(i=0; i<17; i+=16) {
            for(j=i; j<((i==0) ? 8 : 32); j++) {
                if(j+subregs_to_use-1 < i+((i==0) ? 8 : 32)) {          // stay within region
                    if(subregs_free(j, subregs_to_use) && 0x1000 < best_fit) {
                        best_fit = subregs_to_use * 0x200;
                        best_idx = j;
                    }
                }
            }
        }
        if(best_idx == -1) {                                             // No space found, check 8k regions
            subregs_to_use = (size_in_bytes + 1024 - 1)/1024;
            for(i=8; i<33; i+=24) {
                for(j=i; j<8+i; j++) {
                    if(j+subregs_to_use-1 < 8+i) {
                        if(subregs_free(j, subregs_to_use) && 0x2000 < best_fit) {
                            best_fit = subregs_to_use * 0x400;
                            best_idx = j;
                        }
                    }
                }
            }
        }
    }
    else if(size_in_bytes > 0x1000 && size_in_bytes <= 0x2000) {        // size <= 8192 -> Regions R1, R4
        subregs_to_use = (size_in_bytes + 1024 - 1)/1024;
        for(i=8; i<33; i+=24) {
            for(j=i; j<8+i; j++) {
                if(j+subregs_to_use-1 < 8+i) {                          // stay within region
                    if(subregs_free(j, subregs_to_use) && 0x2000 < best_fit) {
                        best_fit = subregs_to_use * 0x400;
                        best_idx = j;
                    }
                }
            }
        }
    }

    if(best_idx == -1)                                                  // Unable to find space
        return 0;   

    for(i=0; i<subregs_to_use; i++)
        subRegInUse |= 1ULL << (best_idx + i);                          // Update mask. 1ULL safety for shifting beyond 32 bits

    if(numAllocs < HCB_MAX_SIZE) {                                      // Store meta data of allocation
        HCB_table[numAllocs].ptr = getAddress(best_idx);
        HCB_table[numAllocs].SP = (void *)((uint32_t)getAddress(best_idx) + best_fit);
        HCB_table[numAllocs].size = best_fit;
        HCB_table[numAllocs].PID = getPID();

        numAllocs++;

        return HCB_table[numAllocs-1].ptr;                              // Retunr based address
    }

    return 0;
}

// REQUIRED: add your free code here and update the SRD bits for the current thread
void freeToHeap(void *pMemory)
{
    uint8_t i, j;
    for(i=0; i<numAllocs; i++) {
        if(HCB_table[i].SP == pMemory) {                                // Find task thru matching SP passed with the one stored in HCB_table
            for(j=0; j<numAllocs; j++) {                                // Once found, check if this task has made more allocations via PID, 
                if(i != j && HCB_table[i].PID == HCB_table[j].PID) {
                    freeTask(j);                                        // If so free said allocations
                    numAllocs--;
                }
            }

            freeTask(i);                                                // Free task allocation
            numAllocs--;
            return;
        }
    }
}

void freeTask(uint8_t task) {
    uint8_t i, start;
    uint16_t subreg_size = 0;
    uint16_t subregs_used = 0;

    start = find_SR(HCB_table[task].ptr);                                               // Find starting sub-region

    if((start == 7 || start == 15 || start == 31 ) && HCB_table[task].size == 0x600) {
        subregs_used = 2;
    }
    else {
        if((start >= 8 && start <= 15) || (start >=32 && start <=39)) 
            subreg_size = 1024;
        else 
            subreg_size = 512;

        subregs_used = (HCB_table[task].size + subreg_size - 1)/subreg_size;            // Determine num of sub-regions used
    }

    for(i=0; i<subregs_used; i++)
        subRegInUse &= ~(1ULL << (start + i));                                          // Update mask

    for(i=task; i<numAllocs; i++) {                                                     // Shift entreis in HCB_table
        HCB_table[i].ptr = HCB_table[i+1].ptr;
        HCB_table[i].SP = HCB_table[i+1].SP;
        HCB_table[i].size = HCB_table[i+1].size;
        HCB_table[i].PID = HCB_table[i+1].PID;
    }
}

// HELPER FUNCTIONS

// determined there are contigious subregions needed
bool subregs_free(int8_t idx, uint8_t subregs_to_use) {
    uint8_t i;
    for(i=idx; i<idx+subregs_to_use; i++) {
        if(subRegInUse & (1ULL << i))
            return 0;
    }
    return 1;
}

// Calculate base address based on starting subregion 
void *getAddress(int8_t subReg) {
    uint16_t size = 0;
    uint16_t region_offset = 0;
    uint8_t subReg_offset = 0;

    if(subReg >= 0 && subReg < 8) {         // R0 - 4k
        size = 512;
        subReg_offset = subReg;
        region_offset = 0x0000;
    }
    else if(subReg >= 8 && subReg < 16) {   // R1 - 8k
        size = 1024;
        subReg_offset = subReg%8;
        region_offset = 0x1000;
    }
    else if(subReg >= 16 && subReg < 24) {  // R2 - 4k
        size = 512;
        subReg_offset = subReg%16;
        region_offset = 0x3000;
    }
    else if(subReg >= 24 && subReg < 32) {  // R2 - 4k
        size = 512;
        subReg_offset = subReg%24;
        region_offset = 0x4000;
    }
    else if(subReg >= 32 && subReg < 40) {  // R2 - 8k
        size = 1024;
        subReg_offset = subReg%32;
        region_offset = 0x5000;
    }

    void *add = (void *)(BASE_ADD + region_offset + subReg_offset*size);
    return add;
}

// Determine subregion index having the base address
int8_t find_SR(void *ptr) {
    uint32_t add = (uint32_t)ptr - BASE_ADD;
    uint16_t size = 0;
    uint16_t region_offset = 0;
    uint8_t region = 0;
    uint8_t subReg_offset = 0;
    uint8_t subReg  = 0;

    if(add < 0x1000) {                         // Region 0 - 4k
        region = 0;
        size = 512;
        region_offset = 0x0000;
    }
    else if(add >= 0x1000 && add < 0x3000) {    // Region 1 - 8k
        region = 1;
        size = 1024;
        region_offset = 0x1000;
    }
    else if(add >= 0x3000 && add < 0x4000) {    // Region 2 - 4k
        region = 2;
        size = 512;
        region_offset = 0x3000;
    }
    else if(add >= 0x4000 && add < 0x5000) {    // Region 3 - 4k
        region = 3;
        size = 512;
        region_offset = 0x4000;
    }
    else if(add >= 0x5000 && add < 0x7000) {    // Region 4 - 8k
        region = 4;
        size = 1024;
        region_offset = 0x5000;
    }

    subReg_offset = (add - region_offset) / size;
    subReg = region*8 + subReg_offset;

    return subReg;
}
//----------------------------------------------------------------------------------------------

// REQUIRED: include your solution from the mini project
void allowFlashAccess(void)
{
    NVIC_MPU_NUMBER_R = 0x5;
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & FLASH_BASE;           // N = 18, add is bits 31-18
                                                                    // size = N-1 => 17
    //          Exec enb | RW priv & unpriv | flash | size | enable region
    NVIC_MPU_ATTR_R |= 0x0<<28 | 0x3<<24 | 0x2<<16 | 0x11<<1 | 0x1;
}

void allowPeripheralAccess(void)
{
    NVIC_MPU_NUMBER_R = 0x6;
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & PERIP_BASE;           // N = 26, base_add bits 31-26
                                                                    // size = N-1 => 25
    //          Exec dis | RW priv & unpriv | periph | size | enable region
    NVIC_MPU_ATTR_R |= 0x0<<28 | 0x3<<24 | 0x5<<16 | 0x19<<1 | 0x1;
}

void setupSramAccess(void)
{
    NVIC_MPU_NUMBER_R = 0x0;                                                            // R0 - 4k
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & R0_4k;                                    // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;   // Exec dis | Full Access | sram | SRDs dis |size | enable region

    NVIC_MPU_NUMBER_R = 0x1;                                                            // R1 - 8k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R1_8k;                                     // N = 13 & size = 12
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xC<<1 | 0x1;   // Exec dis | Full Access | sram | SRDs dis |size | enable region

    NVIC_MPU_NUMBER_R = 0x2;                                                            // R2 - 4k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R2_4k;                                     // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;   // Exec dis | Full Access | sram | SRDs dis |size | enable region

    NVIC_MPU_NUMBER_R = 0x3;                                                            // R3 - 4k
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_ADDR_M & R3_4k;                                     // N = 12 & size = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xB<<1 | 0x1;   // Exec dis | Full Access | sram | SRDs dis |size | enable region

    NVIC_MPU_NUMBER_R = 0x4;                                                            // R4 - 8k
    NVIC_MPU_BASE_R |= NVIC_MPU_BASE_ADDR_M & R4_8k;                                    // N = 13 & size = 12
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | 0x3<<24 | 0x6<<16 | 0xC<<1 | 0x1;   // Exec dis | Full Access | sram | SRDs dis |size | enable region
}

uint64_t createNoSramAccessMask(void)
{
    return 0xFFFFFFFFFFFFFFFF;
}

void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes)
{
    uint8_t i = 0;
    uint8_t start = find_SR(baseAdd);                                               // Find strating subregion index
    uint8_t end = start;
    uint16_t subreg_size = 0;
    uint16_t added_size = 0;

    while(added_size < size_in_bytes) {                                             // determined nymber of subregions to use
        if((end >= 8 && end <= 15) || (end >=32 && end <=39)) 
            subreg_size = 1024;
        else 
            subreg_size = 512;

        added_size += subreg_size;
        end++;
    }

    for(i=0; i<(end-start); i++)                                                    // Update srdBitMask according to start index and number of subregions to use
        *srdBitMask &= ~(1ULL << (start + i));

    //applySramAccessMask(*srdBitMask);
}

void applySramAccessMask(uint64_t srdBitMask)
{
//    uint8_t i;
//    for(i=0; i<5; i++) {
//        NVIC_MPU_NUMBER_R = i;
//        NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> i*8) & 0xFF) << 8);
//        // preserve configuration, clear and then set SRD bits
//    }
// code above was slow

    // Loops seem to be slower
    // One Register  at a time seems to be faster
    // Select Region
    // Preserve configuration & then apply access to respective sub-regions
    NVIC_MPU_NUMBER_R = 0;
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> 0) & 0xFF) << 8);

    NVIC_MPU_NUMBER_R = 1;
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> 8) & 0xFF) << 8);

    NVIC_MPU_NUMBER_R = 2;
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> 16) & 0xFF) << 8);

    NVIC_MPU_NUMBER_R = 3;
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> 24) & 0xFF) << 8);

    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_R & ~NVIC_MPU_ATTR_SRD_M) | (((srdBitMask >> 32) & 0xFF) << 8);
}

