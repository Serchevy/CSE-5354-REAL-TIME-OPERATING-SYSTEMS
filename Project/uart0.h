// UART0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef UART0_H_
#define UART0_H_

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

#define MAX_CHARS 80
#define MAX_FIELDS 6

typedef struct _COMMAND_DATA {
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} COMMAND_DATA;

void initUart0();
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void putcUart0(char c);
void putsUart0(char* str);
char getcUart0();
bool kbhitUart0();

void getsUart0(COMMAND_DATA *data);
void parseFields(COMMAND_DATA *data);
char* getFieldString(COMMAND_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(COMMAND_DATA* data, uint8_t fieldNumber);
bool isCommand(COMMAND_DATA* data, const char strCommand[],uint8_t minArguments);
bool strgcmp(char *str1, const char str2[]);
void itos(uint32_t num, char *str, bool hex, uint8_t hexLen);
void strgcopy(char *dest, const char source[]);

void display(char* txt, uint32_t n, bool hex, uint8_t len);
void putsPidKilled(uint32_t pid);
void printPS(char *name, uint32_t pid, uint16_t cpu, uint8_t state, uint8_t sem, uint8_t mtx);
void printMem(uint32_t pid, uint32_t baseAdd, uint16_t size);
void printSem(uint8_t sema, uint8_t count, uint8_t qSize, uint32_t q[]);
void printMtx(uint8_t mtx, bool locked, uint32_t lockBy, uint8_t qSize, uint32_t q[]);

#endif
