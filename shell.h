// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"

typedef struct _PS_DATA
{
    uint32_t pid[12];
    char name[12][16];
} PS_DATA;

typedef struct _IPCS_MUT_DATA
{
    bool lock;
    uint8_t queueSize;
    char queueNames[2][16];
    char lockedByName[16];
} IPCS_MUT_DATA;

typedef struct _IPCS_SEM_DATA
{
    uint8_t count;
    uint8_t queueSize;
    char queueNames[2][16];
} IPCS_SEM_DATA;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void printHelp(void);
void ps(void);
void ipcs(void);
void kill(uint8_t pid);
void Pkill(const char name[]);
void reboot(void);
void pidof(const char name[]);
void run(const char name[]);
void preempt(bool on);
void sched(bool prio_on);
void shell(void);

#endif
