// Memory manager functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef MM_H_
#define MM_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>

#define NUM_SRAM_REGIONS 4

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void * mallocFromHeap(uint32_t size_in_bytes);
void setupAllAccess(void);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
void initMpu(void);
void generateSramSrdMasks(uint8_t srdMask[NUM_SRAM_REGIONS], void *p, uint32_t size_in_bytes);
void applySramSrdMasks(uint8_t srdMask[NUM_SRAM_REGIONS]);

#endif
