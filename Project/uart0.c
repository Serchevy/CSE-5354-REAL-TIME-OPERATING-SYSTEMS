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

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "gpio.h"
#include "kernel.h"

// Pins
#define UART_TX PORTA,1
#define UART_RX PORTA,0

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0(void)
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    _delay_cycles(3);
    enablePort(PORTA);

    // Configure UART0 pins
    selectPinPushPullOutput(UART_TX);
    selectPinDigitalInput(UART_RX);
    setPinAuxFunction(UART_TX, GPIO_PCTL_PA1_U0TX);
    setPinAuxFunction(UART_RX, GPIO_PCTL_PA0_U0RX);

    // Configure UART0 with default baud rate
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                     // use system clock (usually 40 MHz)
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate;   // calculate divisor (r) in units of 1/128,
                                                        // where r = fcyc / 16 * baudRate
    divisorTimes128 += 1;                               // add 1/128 to allow rounding
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_IBRD_R = divisorTimes128 >> 7;                // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128) >> 1) & 63;       // set fractional value to round(fract(r)*64)
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // turn-on UART0
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF);               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0(void)
{
    while (UART0_FR_R & UART_FR_RXFE);               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

// Returns the status of the receive buffer
bool kbhitUart0(void)
{
    return !(UART0_FR_R & UART_FR_RXFE);
}


// Additional Functions
bool strgcmp(char *str1, const char str2[]) {
    uint32_t i = 0;
    while(str1[i] != 0) {           // While not at the end of passed str
        if(str1[i] != str2[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

void getsUart0(COMMAND_DATA *data) {
    int count = 0;

    while(1) {
        if(!kbhitUart0()) {
            yield();
            continue;
        }
        char c = getcUart0();

        if( (c == 8 || c == 127 ) && (count > 0) ) {    // if backspace decrement count
            count--;
        }
        else if ( c == 13 ) {                         // if new line (Enter key) is entered, return
            data->buffer[count] = 0;
            return;
        }
        else if( c >= 32) {                           // Any printable characters, keep in buffer
            data->buffer[count] = c;
            count++;

            if(count>=MAX_CHARS) {
                data->buffer[count] = 0;
                return;
            }
        }
    }
}


void parseFields(COMMAND_DATA *data) {
    // Numeric 48-57
    // Alpha 65-90 and 97-122
    // Everything else is a delimiter

    char prev = 0;                              // assume previous char is delimiter
    int i = 0;
    data->fieldCount = 0;

    char curr;

    while(data->buffer[i] != 0) {

        curr = data->buffer[i];

        if( !((prev>=65 && prev<=90) || (prev>=97 && prev<=122) || (prev>=48 && prev<=57) || prev == '-') ) {    // Check if previous char IS a delimiter
            if( (curr>=65 && curr<=90) || (curr>=97 && curr<=122) || (curr>=48 && curr<=57 || curr == '-') ) {   // Check if current char is NOT delimiter

                if( (curr>=65 && curr<=90) || (curr>=97 && curr<=122) )
                    data->fieldType[data->fieldCount] = 'a';

                else if( (curr>=48 && curr<=57) || curr == '-' )
                    data->fieldType[data->fieldCount] = 'n';

                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
            }
        }

        prev = curr;

        if(data->fieldCount >= MAX_FIELDS ) {
            break;
        }
        i++;
    }

    i = 0;

    while(data->buffer[i] != 0) {
        if( !((data->buffer[i]>=65 && data->buffer[i]<=90) || (data->buffer[i]>=97 && data->buffer[i]<=122)) ) {    // if its not a letter
            if( !(data->buffer[i]>=48 && data->buffer[i]<=57) && data->buffer[i] != '-' ) {                                                   // and not a number
                data->buffer[i] = 0;                                                                                // set it to NULL
            }
        }
        i++;
    }

    return;
}

char* getFieldString(COMMAND_DATA* data, uint8_t fieldNumber) {

    if(fieldNumber < data->fieldCount) {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    } else {
        return '\0';                                                // Previously had NULL
    }
}

int32_t getFieldInteger(COMMAND_DATA* data, uint8_t fieldNumber) {

    int32_t num = 0;

    if(fieldNumber < data->fieldCount) {
        if((data->fieldType[fieldNumber]) == 'n') {
            int temp = 0;
            int i = 0;
            bool neg = false;
            bool hex = false;
            char *c = &data->buffer[data->fieldPosition[fieldNumber]];

            // handle negative #s
            if(c[i] == '-') {
                neg = true;
                i++;
            }

            if(c[i] == '0' && c[i+1] == 'x') {          // check if hex
                hex = true;
                i += 2;                                 // if so skip 0 & x
            }

            while (c[i] != 0) {
                // decimals
                if(c[i] >= '0' && c[i] <= '9')
                    temp = c[i] - 48;
                // hex
                else if(hex && ((c[i] >= 'A' && c[i] <= 'F') || (c[i] >= 'a' && c[i] <= 'f')))
                    temp = (c[i] | 0x20) - 'a' + 10;    // convert A-F to 10-15 accordingly.

                if(i) {
                    num = num* ((hex) ? 16 : 10);       // Convert based on base
                }

                num += temp;
                i++;
            }

            if(neg) num = -num;

            return num;
        }
    }

    return num;
}

bool isCommand(COMMAND_DATA* data, const char strCommand[], uint8_t minArguments) {

    if(data->buffer[0] == 0) return 0;

    uint32_t i = 0, j = 0;
    while(strCommand[i] != 0) { i++; }
    while(data->buffer[j] != 0) { j++; }

    if(i != j) return 0;                                            // same length?

    uint32_t k = 0;
    while(strCommand[k] != 0) {
        if( !(strCommand[k] == data->buffer[k]) ) {                 // While not at the end strings compare chars
            return 0;                                               // Mismatch, return 0;
        }
        k++;
    }

    if (data->fieldCount-1 == minArguments) {                       // required # of args?
        return 1;
    }

    return 0;
}

void itos(uint32_t num, char *str, bool hex, uint8_t hexLen) {

    uint32_t temp = num;
    uint32_t len = 0;

    if(num == 0) len++;                     // 0 unique case

    if(!hex) {                              // Binary

        while(temp != 0) {                  // get number of digits in num
            temp = temp/10;
            len++;
        }

        str[len] = 0;                       // NULL at end of str

        while(len != 0) {
            str[len-1] = num % 10 + 48;     // convert each digit and add 48
            num = num/10;
            len--;
        }
    }
    else {                                                      // HEX
        len = hexLen;                                           // set length

        str[len] = 0;

        while(len != 0) {
           if((num % 16) < 10 ) str[len-1] = num % 16 + 48;     // if between 0-9 add 48
           else str[len-1] = num % 16 + 55;                     // above, add 65-10 = 55
           num = num/16;
           len--;
        }
    }
}

void strgcopy(char *dest, const char source[]) {
    uint8_t i = 0;
    while(source[i] != 0) {
        dest[i] = source[i];
        i++;
    }
    dest[i] = 0;        // Make sure to copy over end of source str
}

void display(char* txt, uint32_t n, bool hex, uint8_t len) {
    char str[10] = {0};
    itos(n, str, hex, len);
    putsUart0(txt);
    putsUart0(str);
    putsUart0("\n");
}

void putsPidKilled(uint32_t pid) {
    char str[20];
    itos(pid, str, 1, 4);
    putsUart0("0x");
    putsUart0(str);
    putsUart0(" killed\n\n");
}

void printPS(char *name, uint32_t pid, uint16_t cpu, uint8_t state, uint8_t sem, uint8_t mtx) {
    char str[15];
    uint8_t i = 0;
    uint16_t cpu1 = cpu;
    uint16_t cpu2 = cpu;

    // Name
    while(name[i] != 0) i++;
    putsUart0(name);
    if(i<8) putsUart0("\t\t");
    else putsUart0("\t");

    // PID
    itos(pid, str, 1, 4);
    putsUart0("0x");
    putsUart0(str);
    putsUart0("\t ");

    // %CPU
    cpu1 /= 100;    // Upper 2 digits
    cpu2 %= 100;    // Lower 2 digits

    itos(cpu1, str, 0, 0);
    putsUart0(str);
    putsUart0(".");
    itos(cpu2, str, 0, 0);
    if(cpu2 < 10) putsUart0("0");
    putsUart0(str);
    putsUart0("%");
    putsUart0("\t");

    // STATE
    switch(state) {
        case 0:
            putsUart0("  Invalid\n");
            break;
        case 1:
            putsUart0("  Stopped\n");
            break;
        case 2:
            putsUart0("  Ready\n");
            break;
        case 3:
            putsUart0("  Delayed\n");
            break;
        case 4:
            putsUart0("  B-Mutex          ");
            itos(mtx, str, 0, 0);
            putsUart0(str);
            putsUart0("\n");
            break;
        case 5:
            putsUart0("  B-Semaph    ");
            itos(sem, str, 0, 0);
            putsUart0(str);
            putsUart0("\n");
            break;
        default:
            putsUart0("should NOT get here\n");
            break;
    }
}

void printMem(uint32_t pid, uint32_t baseAdd, uint16_t size) {
    char str[15];

    // PID
    itos(pid, str, 1, 4);
    putsUart0("0x");
    putsUart0(str);
    putsUart0("\t\t");


    // Base Address
    itos(baseAdd, str, 1, 8);
    putsUart0("0x");
    putsUart0(str);
    putsUart0("\t");

    // Size
    itos(size, str, 0, 0);
    putsUart0(str);
    putsUart0("\n");
}

void printSem(uint8_t sema, uint8_t count, uint8_t qSize, uint32_t q[]) {
    char str[15];
    uint8_t i;

    // Semaphore #
    itos(sema, str, 0, 0);
    putsUart0(str);
    putsUart0("\t\t");

    // Count
    itos(count, str, 0, 0);
    putsUart0(str);
    putsUart0("\t\t");

    // Queue Size
    itos(qSize, str, 0, 0);
    putsUart0(str);
    putsUart0("\t");

    // Queue
    for(i=0; i<qSize; i++) {
        itos(q[i], str, 1, 4);
        putsUart0("0x");
        putsUart0(str);
    }

    putsUart0("\n");
}


void printMtx(uint8_t mtx, bool locked, uint32_t lockBy, uint8_t qSize, uint32_t q[]) {
    char str[15];
    uint8_t i = 0;

    // Mutex
    itos(mtx, str, 0, 0);
    putsUart0(str);
    putsUart0("\t");

    // Locked
    if(locked) putsUart0("Locked\t");
    else putsUart0("Unlocked\t");

    // Locked by
    itos(lockBy, str, 1, 4);
    putsUart0("0x");
    putsUart0(str);
    putsUart0("\t\t");

    // Queue Size
    itos(qSize, str, 0, 0);
    putsUart0(str);
    putsUart0("\t");

    // Queue
    for(i=0; i<qSize; i++) {
        itos(q[i], str, 1, 4);
        putsUart0("0x");
        putsUart0(str);
    }

    putsUart0("\n");
}



