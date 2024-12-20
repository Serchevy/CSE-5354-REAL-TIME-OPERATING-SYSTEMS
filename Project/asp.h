#ifndef ASP_H_
#define ASP_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

extern void setPSP(uint32_t *add);
extern void setASP();
extern setTMPL();
extern uint32_t *getPSP();
extern uint32_t *getMSP();

extern void pushRegsOnPSP();
extern void popRegsOnPSP();

extern uint32_t getR0();

#endif
