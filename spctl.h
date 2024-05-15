#ifndef SPCTL_H_
#define SPCTL_H_

#include <stdint.h>
#include <kernel.h>

void usePsp(void);
void setPsp(uint32_t psp);
uint32_t *getPsp(void);
uint32_t *getMsp(void);
void *setMsp(uint32_t msp);
void setPcTmpl(void *);
void strRegs(void);
void ldrRegs(void);
void runProgram(void *pc);
void strPid(uint32_t pid);

#endif
