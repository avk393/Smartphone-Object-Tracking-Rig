// UARTIntsTestMain.c
// Runs on LM4F120/TM4C123
// Tests the UART0 to implement bidirectional data transfer to and from a
// computer running HyperTerminal.  This time, interrupts and FIFOs
// are used.

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "SysTick.h"
#include "PLL.h"
#include "Timer0A.h"
#include "UART.h"
#include "stepper.h"
#include "useless.h"

#define PF2             (*((volatile uint32_t *)0x40025010))
#define PF1             (*((volatile uint32_t *)0x40025008))
	// if desired interrupt frequency is f, Timer0A_Init parameter is busfrequency/f
#define F8HZ (80000000/8)
#define F20KHZ (50000000/20000)
#define F24HZ (80000000/24)
#define T1ms						3600

//void EnableInterrupts(void);
//void DisableInterrupts(void);

uint32_t motorX, motorY, delay;
int32_t motorCheckX, motorCheckY;
//int32_t flag = 1;
//int32_t counter = 0;
uint32_t delayX = 80000;
uint32_t delayY = 80000;
//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}
 
// Make PF2 an output, enable digital I/O, ensure alt. functions off
void PortF_Init(void){ 
	SYSCTL_RCGCGPIO_R |= 0x20;        // 1) activate clock for Port F
  while((SYSCTL_PRGPIO_R&0x20)==0){}; // allow time for clock to start
		
	GPIO_PORTF_DIR_R |= 0x06;             // make PF2, PF1 out (built-in LED)
  GPIO_PORTF_AFSEL_R &= ~0x06;          // enable digital I/O on PF2, PF1
  GPIO_PORTF_DEN_R |= 0x06;             // configure PF2 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF00F)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;               // disable analog functionality on PF
}

void MotorTask() {
		uint32_t num = UART_InUHex(2);
		motorX = num/10;
		//delayX = (num%10)+1;
		//delayX = delayX*(-144000) + 1600000;
		motorY = num%10;
		//motorY = (num%100)/10;
		//delayY = num%10;
		//delayX = delayX*(-144000) + 1600000;
		PF2 ^= 0x04;
}

//debug code
int main(void){
	PortF_Init();
	//PF2^= 0x04;
  PLL_Init(Bus80MHz);       // set system clock to 80 MHz
	Stepper_Init();
	UART_Init();              // initialize UART

	// Timer0A_Init(&MotorTask, F24HZ);
	// 360 steps on x motor is 180 degree spin
	// 360 steps on y motor is 90 degree spin
	
	motorX = 0;
	motorY = 0;
	
	
  while(1){
		// 25 delay for 50 MHz bus speed
		// 50 delay for 80 MHz bus speed
		if(motorX==1){
			StepperX_CCW(50*T1ms);
			motorCheckX++;
		}
		else if(motorX==2){
			StepperX_CW(50*T1ms);
			motorCheckX--;
		}
		
		if(motorY==1){
			StepperY_CW(50*T1ms);
			motorCheckY++;
		}
		else if(motorY==2){
			StepperY_CCW(50*T1ms);
			motorCheckY--;
		}
		SysTick_Wait(800000);
		//StepperX_CW(50*T1ms);
		//StepperY_CW(50*T1ms);
		//SysTick_Wait(8000);
		//PF2 ^= 0x04;
	
  }
}
