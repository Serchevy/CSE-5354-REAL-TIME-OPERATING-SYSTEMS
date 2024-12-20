;-----------------------------------------------------------------------------
; Device includes, defines, and assembler directives
;-----------------------------------------------------------------------------

	.def setPSP
	.def setASP
	.def setTMPL
	.def getPSP
	.def getMSP
	.def pushRegsOnPSP
	.def popRegsOnPSP
	.def getR0

;-----------------------------------------------------------------------------
; Register values and large immediate values
;-----------------------------------------------------------------------------

.thumb
.const

setPSP:
		MSR  PSP, R0		; Initialize PSP

setASP:
		MRS	 R0, CONTROL
		ORR  R0, R0, #2
		MSR	 CONTROL, R0	; Set ASP bit
		ISB

		BX   LR

setTMPL:
		MRS  R0, CONTROL
		ORR  R0, R0, #1
		MSR	 CONTROL, R0	; Set TMPL bit
		ISB

		BX   LR


getPSP:
		MRS R0, PSP

		BX  LR

getMSP:
		MRS R0, MSP

		BX  LR


pushRegsOnPSP:
		MRS R0, PSP			; load curr PSP onto R0
		SUB R0, R0, #36		; Make space for the 9 regs -> PSP -= 9*4

		STR R4,  [R0, #32] 	; Store R4 @ PSP. Offset of #0
		STR R5,  [R0, #28]	; R5
		STR R6,  [R0, #24]	; R6
		STR R7,  [R0, #20]	; R7
		STR R8,  [R0, #16]	; R8
		STR R9,  [R0, #12]	; R9
		STR R10, [R0, #8]	; R10
		STR R11, [R0, #4]	; R11

		MOVW R1, #0xFFFD
        MOVT R1, #0xFFFF
		STR  R1, [R0, #0]	; LR

		MSR PSP, R0			; update PSP

		BX  LR


popRegsOnPSP:
		MRS R0, PSP			; load PSP onto R0

		LDR R4,  [R0, #32] 	; Load R4
		LDR R5,  [R0, #28]	; R5
		LDR R6,  [R0, #24]	; R6
		LDR R7,  [R0, #20]	; R7
		LDR R8,  [R0, #16]	; R8
		LDR R9,  [R0, #12]	; R9
		LDR R10, [R0, #8]	; R10
		LDR R11, [R0, #4]	; R11
		LDR LR,  [R0, #0]	; LR

		ADD R0, R0, #36
		MSR PSP, R0			; Update PSP

		BX  LR

getR0:
		BX  LR





