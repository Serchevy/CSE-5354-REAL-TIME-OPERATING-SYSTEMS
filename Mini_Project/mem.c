//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "mem.h"
#include "tm4c123gh6pm.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

HCB HCB_table[12] = {};                 // Up to 12 Apps
uint64_t subRegInUse = 0;               // In use sub-regions mask

uint8_t PID = 0;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool subregs_free(int8_t idx, uint8_t subregs_to_use) {
    uint8_t i;
    for(i=idx; i<idx+subregs_to_use; i++) {
        if(subRegInUse & (1ULL << i))
            return 0;
    }
    return 1;
}

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

void *malloc_from_heap(uint32_t size_in_bytes) {
    if(size_in_bytes > 0x2000 || size_in_bytes == 0x0) return NULL;     // 8kiB cap

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

    if(best_idx == -1) return NULL;                                         // Unable to find space
    for(i=0; i<subregs_to_use; i++)
        subRegInUse |= 1ULL << (best_idx + i);                              // Update mask. 1ULL safety for shifting beyond 32 bits

    for(j=0; j<12; j++) {                                                   // Find task slot not in use
        if(!HCB_table[j].inUse) {                                           // Fill in meta-data
            HCB_table[j].inUse = 1;
            HCB_table[j].ptr = getAddress(best_idx);
            HCB_table[j].size = best_fit;
            HCB_table[j].PID = PID++;

            return HCB_table[j].ptr;
        }
    }
    return NULL;
}

void free_to_heap(void *ptr) {
    uint8_t i, start;
    uint16_t subreg_size = 0;
    uint16_t subregs_used = 0;

    for(i=0; i<12; i++) {
        if(HCB_table[i].ptr == ptr) {

            start = find_SR(ptr);                                                            // Find starting sub-region

            if((start == 7 || start == 15 || start == 31 ) && HCB_table[i].size == 0x600) {
                subregs_used = 2;
            }
            else {
                if((start >= 8 && start <= 15) || (start >=32 && start <=39)) subreg_size = 1024;
                else subreg_size = 512;
                subregs_used = (HCB_table[i].size + subreg_size - 1)/subreg_size;           // Determine num of sub-regions used
            }

            for(i=0; i<subregs_used; i++)
                subRegInUse &= ~(1ULL << (start + i));                                      // Update mask

            HCB_table[i].inUse = 0;                                                         // Clear task meta-data
            HCB_table[i].ptr = NULL;
            HCB_table[i].size = 0;
            HCB_table[i].PID = 0;

            return;
        }
    }
}
