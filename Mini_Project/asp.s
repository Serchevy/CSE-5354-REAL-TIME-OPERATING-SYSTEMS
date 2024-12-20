;-----------------------------------------------------------------------------
; Device includes, defines, and assembler directives
;-----------------------------------------------------------------------------

   .def setPSP
   .def setASP
   .def setTMPL
   .def getPSP
   .def getMSP

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
		MRS R0, CONTROL
		ORR  R0, R0, #1
		MSR	 CONTROL, R0	; Set TMPL bit
		ISB

		BX   LR


getPSP:
		MRS R0, PSP

		BX LR

getMSP:
		MRS R0, MSP

		BX LR
