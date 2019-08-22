// UART.c
// Runs on LM4F120/TM4C123
// Use UART1 to implement bidirectional data transfer 
//This time, interrupts and FIFOs are used.


// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1
#include <stdint.h>
#include "tm4c123gh6pm.h"

#include "FIFO.h"
#include "UART.h"
#include "useless.h"

#define PF2             (*((volatile uint32_t *)0x40025010))
#define PF1             (*((volatile uint32_t *)0x40025008))

#define NVIC_EN0_INT5           0x00000020  // Interrupt 5 enable
#define NVIC_EN0_INT6           0x00000040  // Interrupt 5 enable

#define UART_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define UART_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART_CTL_UARTEN         0x00000001  // UART Enable
#define UART_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define UART_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define UART_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define UART_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define UART_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define UART_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define UART_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define UART_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define UART_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define UART_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define UART_ICR_RXIC           0x00000010  // Receive Interrupt Clear



void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
#define FIFOSIZE   16         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
AddIndexFifo(Rx, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
AddIndexFifo(Tx, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)

// Initialize UART1
// Baud rate is 115200 bits/sec
void UART_Init(void){
  SYSCTL_RCGCUART_R |= 0x02;            // activate UART1
  SYSCTL_RCGCGPIO_R |= 0x02;            // activate port B
  RxFifo_Init();                        // initialize empty FIFOs
  TxFifo_Init();
  UART1_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  //UART1_IBRD_R = 325;                    // IBRD = int(50,000,000 / (16 * 9600)) = int(325.52083)
  //UART1_FBRD_R = 33;                     // FBRD = int(0.52083 * 64 + 0.5) = 8
	UART1_IBRD_R = 520;                    
  UART1_FBRD_R = 53;
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART1_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART1_IFLS_R &= ~0x3F;                // clear TX and RX interrupt FIFO level fields
                                        // configure interrupt for TX FIFO <= 1/8 full
                                        // configure interrupt for RX FIFO >= 1/8 full
  UART1_IFLS_R += (UART_IFLS_TX1_8|UART_IFLS_RX1_8);
                                        // enable TX and RX FIFO interrupts and RX time-out interrupt
  UART1_IM_R |= (UART_IM_RXIM|UART_IM_TXIM|UART_IM_RTIM);
  UART1_CTL_R |= 0x301;                 // enable UART
  GPIO_PORTB_AFSEL_R |= 0x03;           // enable alt funct on PB1-0
  GPIO_PORTB_DEN_R |= 0x03;             // enable digital I/O on PB1-0
                                        // configure PB1-0 as UART
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFFFF00)+0x00000011;
  GPIO_PORTB_AMSEL_R = 0;               // disable analog functionality on PA
                                        // UART1=priority 2
  NVIC_PRI1_R = (NVIC_PRI1_R&0xF00FFFFF)|0x00400000; // bits 13-15
  NVIC_EN0_R = NVIC_EN0_INT6;           // enable interrupt 6 in NVIC
  EnableInterrupts();
}
// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware(void){
  char letter;
  while(((UART1_FR_R&UART_FR_RXFE) == 0) && (RxFifo_Size() < (FIFOSIZE - 1))){
    letter = UART1_DR_R;
		uint32_t test = RxFifo_Put(letter);
  }
}
// copy from software TX FIFO to hardware TX FIFO
// stop when software TX FIFO is empty or hardware TX FIFO is full
void static copySoftwareToHardware(void){
  char letter;
  while(((UART1_FR_R&UART_FR_TXFF) == 0) && (TxFifo_Size() > 0)){
    TxFifo_Get(&letter);
    UART1_DR_R = letter;
  }
}
// input ASCII character from UART
// spin if RxFifo is empty
char UART_InChar(void){
  char letter;
	//uint32_t counter;
  while(RxFifo_Get(&letter) == FIFOFAIL){
		//counter++;
		//if(counter>32) break;
	}
  return(letter);
}
// output ASCII character to UART
// spin if TxFifo is full
void UART_OutChar(char data){
  while(TxFifo_Put(data) == FIFOFAIL){};
  UART1_IM_R &= ~UART_IM_TXIM;          // disable TX FIFO interrupt
  copySoftwareToHardware();
  UART1_IM_R |= UART_IM_TXIM;           // enable TX FIFO interrupt
}
// at least one of three things has happened:
// hardware TX FIFO goes from 3 to 2 or less items
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
//uint32_t counter = 0;
void UART1_Handler(void){
	long i = StartCritical();
	//DisableInterrupts();
  if(UART1_RIS_R&UART_RIS_TXRIS){       // hardware TX FIFO <= 2 items
    UART1_ICR_R = UART_ICR_TXIC;        // acknowledge TX FIFO
    // copy from software TX FIFO to hardware TX FIFO
    copySoftwareToHardware();
    if(TxFifo_Size() == 0){             // software TX FIFO is empty
      UART1_IM_R &= ~UART_IM_TXIM;      // disable TX FIFO interrupt
    }
  }
  if(UART1_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART1_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
		//counter++;
		//if(counter%2==0) 
		MotorTask();
		EndCritical(i);
		//EnableInterrupts();
  }
  if(UART1_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART1_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
		MotorTask();
		EndCritical(i);
		//EnableInterrupts();
  }
}

//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void UART_OutString(char *pt){
  while(*pt){
    UART_OutChar(*pt);
    pt++;
  }
}

//------------UART_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUDec(void){
uint32_t number=0, length=0;
char character;
  character = UART_InChar();
  while(character != CR){ // accepts until <enter> is typed
// The next line checks that the input is a digit, 0-9.
// If the character is not 0-9, it is ignored and not echoed
    if((character>='0') && (character<='9')) {
      number = 10*number+(character-'0');   // this line overflows if above 4294967295
      length++;
      UART_OutChar(character);
    }
// If the input is a backspace, then the return number is
// changed and a backspace is outputted to the screen
    else if((character==BS) && length){
      number /= 10;
      length--;
      UART_OutChar(character);
    }
    character = UART_InChar();
  }
  return number;
}

//-----------------------.0UART_OutUDec-----------------------
// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void UART_OutUDec(uint32_t n){
// This function uses recursion to convert decimal number
//   of unspecified length as an ASCII string
  if(n >= 10){
    UART_OutUDec(n/10);
    n = n%10;
  }
  UART_OutChar(n+'0'); /* n is between 0 and 9 */
}

//---------------------UART_InUHex----------------------------------------
// Accepts ASCII input in unsigned hexadecimal (base 16) format
// Input: none
// Output: 32-bit unsigned number
// No '$' or '0x' need be entered, just the 1 to 8 hex digits
// It will convert lower case a-f to uppercase A-F
//     and converts to a 16 bit unsigned number
//     value range is 0 to FFFFFFFF
// If you enter a number above FFFFFFFF, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUHex(uint32_t size){
uint32_t number=0, digit, length=0;
char character;
  //while(character != CR){
	while (length<size) {
		character = UART_InChar();
    //digit = 0x10; // assume bad\
		digit = 0x00; // assume bad
    if((character>='0') && (character<='9')){
      digit = character-'0';
    }
    /*else if((character>='A') && (character<='F')){
      digit = (character-'A')+0xA;
    }
    else if((character>='a') && (character<='f')){
      digit = (character-'a')+0xA;
    }*/
		length++;
		/*
// If the character is not 0-9 or A-F, it is ignored and not echoed
    if(digit <= 0xF){
      number = number*0x10+digit;
      length++;
      UART_OutChar(character);
    }
// Backspace outputted and return value changed if a backspace is inputted
    else if((character==BS) && length){
      number /= 0x10;
      length--;
      UART_OutChar(character);
    }
		*/
		number = number*(10);
		number = number + digit;
  }
  return number;
}

//--------------------------UART_OutUHex----------------------------
// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void UART_OutUHex(uint32_t number){
// This function uses recursion to convert the number of
//   unspecified length as an ASCII string
  if(number >= 0x10){
    UART_OutUHex(number/0x10);
    UART_OutUHex(number%0x10);
  }
  else{
    if(number < 0xA){
      UART_OutChar(number+'0');
     }
    else{
      UART_OutChar((number-0x0A)+'A');
    }
  }
}

//------------UART_InString------------
// Accepts ASCII characters from the serial port
//    and adds them to a string until <enter> is typed
//    or until max length of the string is reached.
// It echoes each character as it is inputted.
// If a backspace is inputted, the string is modified
//    and the backspace is echoed
// terminates the string with a null character
// uses busy-waiting synchronization on RDRF
// Input: pointer to empty buffer, size of buffer
// Output: Null terminated string
// -- Modified by Agustinus Darmawan + Mingjie Qiu --
void UART_InString(char *bufPt, uint16_t max) {
int length=0;
char character;
	PF2 ^= 0x04;
  character = UART_InChar();
  //while(character != CR){
	while(length<max) {
    if(character == BS){
      if(length){
        bufPt--;
        length--;
        //UART_OutChar(BS);
      }
    }
    else if(length < max){
      *bufPt = character;
      bufPt++;
      length++;
      //UART_OutChar(character);
    }
    character = UART_InChar();
  }
  *bufPt = '\0';
}
