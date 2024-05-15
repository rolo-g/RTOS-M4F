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
#include "uart0.h"
#include "uartio.h"
#include "kernel.h"
#include "mm.h"

uint32_t *addrTable[MAX_TASKS];
uint16_t sizeTable[MAX_TASKS];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add your malloc code here and update the SRD bits for the current thread
void * mallocFromHeap(uint32_t size_in_bytes)
{
    // for now, it is unknown if tasks will be able to assign their own pid nums
    // as such, there can currently only be pid numbers 0 to 31

    char buf[MAX_CHARS];
    uint8_t pid = 0;
    int8_t i = 0;
    void *ptr = (uint32_t *)0x20001000;

    while ((pid <= MAX_TASKS) && addrTable[pid])
        pid++;


    if (pid > MAX_TASKS)
        putsUart0("Too many tasks running, free before creating new task\n\n");
    else if (size_in_bytes > 24576 || size_in_bytes == 0)
        putsUart0("Cannot allocate 0B or anything above 24576B\n\n");
    else
    {
        if (size_in_bytes <= 512)
            size_in_bytes = 512;
        else
        {
            // rounds any value above 1024 UP the nearest multiple of 1024
            size_in_bytes = (((size_in_bytes + 1023) / 1024) * 1024);
            ptr = (uint32_t *)0x20002000;
        }

        // looks through all the pids to make sure none are using this address
        while (i <= MAX_TASKS)
        {

            // checks if ptr is within the current pid's region
            if (((uint32_t)addrTable[i] <= (uint32_t)ptr) &&
                    ((uint32_t)ptr < ((uint32_t)addrTable[i] + sizeTable[i])) ||
                    ((uint32_t)addrTable[i] <= (uint32_t)ptr + size_in_bytes) &&
                    ((uint32_t)ptr + size_in_bytes < ((uint32_t)addrTable[i] + sizeTable[i])))
            {
                // if so, move the ptr pointer to the end of the current pid
                ptr = (uint32_t *)((uint32_t)addrTable[i] + sizeTable[i]);

                // if the 512 MPU is full, make any 512 allocation 1024
                if (size_in_bytes == 512 && ((uint32_t)ptr == 0x20002000))
                {
                    size_in_bytes = 1024;
                    ptr = (uint32_t *)0x20002000;
                    i = -1;
                }
            }
            i++;
        }

        // only allocate if the address range is below 0x20008000
        if ((uint32_t)ptr + size_in_bytes <= 0x20008000)
        {
            addrTable[pid] = ptr;
            sizeTable[pid] = size_in_bytes;

            /*
            putsUart0("Allocating task ");
            putsUart0(IntToString(pid, buf));
            putsUart0(" at location ");
            putsUart0(HexToString((uint32_t)addrTable[pid], buf));
            putsUart0(" with size_in_bytes ");
            putsUart0(IntToString(sizeTable[pid], buf));
            putsUart0("B...\n\n");
            */
        }
        else
        {
            putsUart0("Task is either too large or not enough memory left\n\n");
            ptr = 0x0;
        }
    }

    return ptr;
}

// REQUIRED: add your custom MPU functions here (eg to return the srd bits)
void setupAllAccess(void)
{
    // bg rule 0
    // base addr 0x0 size 4 GiB = 4294967296 B
    // N = log_2(4294967296 B) = 32
    NVIC_MPU_NUMBER_R = 0;
    NVIC_MPU_BASE_R = 0x0;
                                          // RW PR|UNPR   // TEX0 S0 C0 B0   // 32 - 1 = 31
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | (0b011 << 24) | (0b000000 << 16) | (0b11111 << 1) | NVIC_MPU_ATTR_ENABLE;
}

void allowFlashAccess(void)
{
    // base addr 0x0 size 0x00040000 = 262144 B
    // N = log_2(256 KiB) = 18
    NVIC_MPU_NUMBER_R = 1;
    NVIC_MPU_BASE_R = 0x0;
                       // RW PR|UNPR  // TEX0 S0 C1 B0   // 18 - 1 = 17
    NVIC_MPU_ATTR_R |= (0b011 << 24) | (0b000010 << 16) | (0b10001 << 1) | NVIC_MPU_ATTR_ENABLE;
}

void allowPeripheralAccess(void)
{
    // base addr 0x40000000 size 0x4000000 = 67108864 B
    // N = log_2(65536 KiB) = 26
    NVIC_MPU_NUMBER_R = 6;
    NVIC_MPU_BASE_R = 0x40000000;
                       // EXECUTE NEVER   // RW PR|UNPR   // TEX0 S1 C0 B1   // 26 - 1 = 25
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | (0b011 << 24) | (0b000101 << 16) | (0b11001 << 1) | NVIC_MPU_ATTR_ENABLE;
}

void setupSramAccess(void)
{
    // base addr 0x20000000 size 0x8000 = 32768 B67108864
    // we want each MPU to have 8KiB with 1KiB subregions...
    // ... the subregions will take care of themselves. each mpu has 8 regions
    // 32KiB / 8 KiB = 4 MPU regions to configure
    // N = log_2(8192 B) = 13
    // we need to leave 0x20000000 - 0x20000FFF to the OS though
    // thus, we need just one region that covers 4096 B after 0x*FFF
    // N = log_2(4096 B) = 12

    // os protection
    NVIC_MPU_NUMBER_R = 6;
    NVIC_MPU_BASE_R = 0x20000000;
                       // EXECUTE NEVER   // RW PR-ONLY   // TEX0 S1 C1 B0   // 12 - 1 = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | (0b001 << 24) | (0b000110 << 16) | (0b01011 << 1) | NVIC_MPU_ATTR_ENABLE;

    // this region only covers 4096 B, each subregion is 512 B
    NVIC_MPU_NUMBER_R = 2;
    NVIC_MPU_BASE_R = 0x20001000;
                       // EXECUTE NEVER   // RW PR-ONLY   // TEX0 S1 C1 B0   // 12 - 1 = 11
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | (0b001 << 24) | (0b000110 << 16) | (0b01011 << 1) | NVIC_MPU_ATTR_ENABLE;

    // for these three regions, they cover 8192 B, each sub-region is 1024 B
    uint8_t i;
    for (i = 3; i <= 5; i++)
    {
        NVIC_MPU_NUMBER_R = i;
        NVIC_MPU_BASE_R = 0x20002000 + (0x2000 * (i - 3));
                           // EXECUTE NEVER   // RW PR-ONLY   // TEX0 S1 C1 B0   // 13 - 1 = 12
        NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN | (0b001 << 24) | (0b000110 << 16) | (0b01100 << 1) | NVIC_MPU_ATTR_ENABLE;
    }
}

// REQUIRED: initialize MPU here
void initMpu(void)
{
    // Enables fault handlers
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM;

    setupAllAccess();
    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();

    // Enables the MPU
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_ENABLE;
}

void generateSramSrdMasks(uint8_t srdMask[NUM_SRAM_REGIONS], void *p, uint32_t size_in_bytes)
{
    // for now, this function does no error checking to see if
    // the base address is valid. it is assumed that the user
    // is inputting a valid base address. this will obviously change later

    char buf[MAX_CHARS];
    uint8_t srdCount = 0;

    uint32_t *pInit = (uint32_t*)((uint32_t)p - (size_in_bytes - 1));
    uint32_t *ptr = pInit;
    uint16_t scale = 512;
    uint8_t shift = 0;
    uint8_t MPU = 2;

    // if the base addr starts at MPU region 3, make scale 1024
    if ((uint32_t)pInit >= 0x20002000)
        scale = 1024;

    if ((uint32_t)pInit >= 0x20000000 && (uint32_t)pInit <= 0x20008000)
    {
        // skips over first 4k since its already RW
        if ((uint32_t)pInit < 0x20001000)
            ptr = (uint32_t *)0x20001000;

        while ((uint32_t)ptr < (uint32_t)pInit + size_in_bytes)
        {
            // MPU 2
            if ((uint32_t)ptr >= 0x20001000 && (uint32_t)ptr < 0x20001FFF)
            {
                shift = ((uint32_t)ptr - 0x20001000) / scale;
                srdMask[MPU - 2] |= (0b1 << shift);

                srdCount++; // remove on final

                if ((uint32_t) ptr >= 0x20001E00)
                    scale = 1024;
            }

            // MPUs 3 - 5
            if ((uint32_t)ptr >= 0x20002000 && (uint32_t)ptr < 0x20007FFF)
            {
                MPU = ((uint32_t)ptr - 0x20000000) / 0x2000 + 2;
                shift = (((uint32_t)ptr - 0x20002000) - (0x2000 * (MPU - 3))) / scale;
                srdMask[MPU - 2] |= (0b1 << shift);

                srdCount++; // remove on final

            }
            ptr += (scale / 4);
        }

        /*
        putsUart0(IntToString(srdCount, buf));
        putsUart0(" sub-region(s) starting at ");
        putsUart0(HexToString((uint32_t)pInit, buf));
        putsUart0(" now have R/W access for users\n\n");
        */
    }
    else
        putsUart0("No\n\n");
}

void applySramSrdMasks(uint8_t srdMask[NUM_SRAM_REGIONS])
{
    uint8_t i = 0;

    for (i = 0; i < NUM_SRAM_REGIONS; i++)
    {
        NVIC_MPU_NUMBER_R = i + 2;
        NVIC_MPU_ATTR_R &= ~0xFF00;
        NVIC_MPU_ATTR_R |= (srdMask[i] << 8);
    }
}
