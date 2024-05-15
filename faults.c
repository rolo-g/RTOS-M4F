// Shell functions
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
#include "spctl.h"
#include "faults.h"

uint16_t pid = 0;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: If these were written in assembly
//           omit this file and add a faults.s file

// REQUIRED: code this function
void mpuFaultIsr(void)
{                        // r0      // r1            // r2
    uint32_t stack[8] = {*getPsp(), *(getPsp() + 1), *(getPsp() + 2),
                         // r3            // r12           // lr
                         *(getPsp() + 3), *(getPsp() + 4), *(getPsp() + 5),
                         // pc            // xpsr
                         *(getPsp() + 6), *(getPsp() + 7)};
    uint32_t *psp = getPsp();
    uint32_t *msp = getMsp();

    char buf[MAX_CHARS];

    putsUart0("MPU fault in process ");
    putsUart0(IntToString(pid, buf));
    putsUart0("\n");

    putsUart0(" PSP \t\t");
    putsUart0(HexToString((uint32_t)psp, buf));
    putsUart0("\n");

    putsUart0(" MSP \t\t");
    putsUart0(HexToString((uint32_t)msp, buf));
    putsUart0("\n");

    putsUart0(" MFAULT FLAGS\t");
    putsUart0(HexToString(NVIC_FAULT_STAT_R & 0x000000FF, buf));
    putsUart0("\n");

    putsUart0(" MEM ADDR\t");
    putsUart0(HexToString(NVIC_MM_ADDR_R, buf));
    putsUart0("\n");

    putsUart0(" xPSR\t\t");
    putsUart0(HexToString((uint32_t)stack[7], buf));
    putsUart0("\n");

    putsUart0(" PC\t\t");
    putsUart0(HexToString((uint32_t)stack[6], buf));
    putsUart0("\n");

    putsUart0(" LR\t\t");
    putsUart0(HexToString((uint32_t)stack[5], buf));
    putsUart0("\n");

    putsUart0(" R0\t\t");
    putsUart0(HexToString((uint32_t)stack[0], buf));
    putsUart0("\n");

    putsUart0(" R1\t\t");
    putsUart0(HexToString((uint32_t)stack[1], buf));
    putsUart0("\n");

    putsUart0(" R2\t\t");
    putsUart0(HexToString((uint32_t)stack[2], buf));
    putsUart0("\n");

    putsUart0(" R3\t\t");
    putsUart0(HexToString((uint32_t)stack[3], buf));
    putsUart0("\n");

    putsUart0(" R12\t\t");
    putsUart0(HexToString((uint32_t)stack[4], buf));
    putsUart0("\n");

    putsUart0("\n");

    NVIC_SYS_HND_CTRL_R |= !NVIC_SYS_HND_CTRL_MEMP;
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

// REQUIRED: code this function
void hardFaultIsr(void)
{
    uint32_t *psp = getPsp();
    uint32_t *msp = getMsp();

    char buf[MAX_CHARS];

    putsUart0("Hard fault in process ");
    putsUart0(IntToString((uint32_t)pid, buf));
    putsUart0("\n");

    putsUart0(" PSP \t\t");
    putsUart0(HexToString((uint32_t)psp, buf));
    putsUart0("\n");

    putsUart0(" MSP \t\t");
    putsUart0(HexToString((uint32_t)msp, buf));
    putsUart0("\n");

    putsUart0(" HFAULT FLAGS\t");
    putsUart0(HexToString(NVIC_HFAULT_STAT_R, buf));
    putsUart0("\n");

    putsUart0("\n");

    while(true);
}

// REQUIRED: code this function
void busFaultIsr(void)
{
    char buf[MAX_CHARS];

    putsUart0("Bus fault in process ");
    putsUart0(IntToString(pid, buf));
    putsUart0("\n\n");

    while(true);
}

// REQUIRED: code this function
void usageFaultIsr(void)
{
    char buf[MAX_CHARS];

    putsUart0("Usage fault in process ");
    putsUart0(IntToString(pid, buf));
    putsUart0("\n\n");

    while(true);
}
