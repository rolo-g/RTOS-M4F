; 4354 stack pointer control functions for RTOS
; Rolando Rosales 1001850424

	.def usePsp
	.def setPsp
	.def getPsp
	.def getMsp
	.def setMsp
	.def setPcTmpl
	.def strRegs
	.def ldrRegs
	.def runProgram
	.def strPid

.thumb
.const

XPSR_VALID					.field 0x01000000
EXC_RETURN_NON_FLOAT_PSP	.field 0xFFFFFFFD

.text

usePsp:
	MRS R0, CONTROL	; load control reg into R0
	ISB
    ORR	R0, R0, #2	; set ASP bit (2^1) to 1
    MSR	CONTROL, R0 ; load R0 back into control reg
    ISB				; ISB needed after MSR
    BX  LR

setPsp:
	MSR PSP, R0		; loads the address from R0 into the PSP
	ISB
	BX 	LR

getPsp:
	MRS R0, PSP		; loads the address from the PSP into R0
	ISB
	BX LR

getMsp:
	MRS R0, MSP		; loads the address from the MSP into R0
	ISB
	BX LR

setMsp:
	MSR MSP, R0		; loads the address from the MSP into R0
	ISB
	BX LR

setPcTmpl:
	MRS R1, CONTROL	; load control reg into R1
	ISB
    ORR	R1, R1, #1	; set TMPL/nPRIV bit (2^0) to 1
    MSR	CONTROL, R1 ; load R1 back into control reg
    ISB				; ISB needed after MSR
	MOV PC, R0		; move the addr of task 0 into PC
	BX R0

strRegs:
	PUSH {LR}
    MRS    R0, PSP
    ISB
    STMDB  R0!, {R4-R11, LR}
    MSR    PSP, R0
    ISB
    POP {LR}
    BX LR

ldrRegs:
	PUSH {LR}
    MRS    R0, PSP 				; gets psp
	ISB
    LDMIA  R0!, {R4-R11, LR}	; stores r4-r11 and lr
    MSR    PSP, R0				; stores r0 back into psp
    ISB
    POP {LR}
    BX LR

runProgram:
	MRS R2, PSP							; store PSP into R2
	ISB
	LDR R1, XPSR_VALID 					; load 0x01000000 into R1
	STR R1, [R2], #-4 					; store r1 into the xPSR
	STR R0, [R2], #-4 					; store passed value (pc) into R0
	STMDB  R2!, {LR, R12, R3, R2, R1} 	; store random junk into LR-R1
	STR R0, [R2] 						; stores in R0, not w/ STMDB to not dec
	MSR PSP, R2 						; writes R2 back into PSP
	ISB
	LDR R0, EXC_RETURN_NON_FLOAT_PSP 	; loads 0xFFFFFFFD into R0
	MOV LR, R0 							; makes LR use non-float PSP return
	BX LR

strPid:
    MRS    R1, PSP
    ISB
    STR	   R0, [R1]
	MSR	   PSP, R1
	ISB
	BX LR

.endm
