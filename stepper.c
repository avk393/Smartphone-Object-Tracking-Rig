// Stepper.c
// Runs on LM4F120/TM4C123
// Provide functions that step the motor once clockwise, step
// once counterclockwise, and initialize the stepper motor
// interface.

// PD3 connected to driver for stepper motor coil A
// PD2 connected to driver for stepper motor coil A'
// PD1 connected to driver for stepper motor coil B
// PD0 connected to driver for stepper motor coil B'

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "systick.h"
struct State{
  uint8_t Out;           // Output
  const struct State *Next[2]; // CW/CCW
};
typedef const struct State StateType;
typedef StateType *StatePtr;
#define clockwise 0        // Next index
#define counterclockwise 1 // Next index
StateType fsm[4]={
  {10,{&fsm[1],&fsm[3]}},
  { 9,{&fsm[2],&fsm[0]}},
  { 5,{&fsm[3],&fsm[1]}},
  { 6,{&fsm[0],&fsm[2]}}
};
uint8_t PosX;     // between 0 and 199
uint8_t PosY;
const struct State *PtX;	// Current horizontal motor State
const struct State *PtY;	// Current vertical motor State

#define PE4	(*((volatile uint32_t *)0x40024040))
#define STEPPERY  (*((volatile uint32_t *)0x4000703C))		// PD0-3
#define STEPPERX  (*((volatile uint32_t *)0x4002403C))		// PE0-3
// Move 1.8 degrees clockwise, delay is the time to wait after each step
void StepperX_CW(uint32_t delay){
  PtX = PtX->Next[clockwise];     // circular
	STEPPERX = PtX->Out; // step motor
  if(PosX==199){      // shaft angle
    PosX = 0;         // reset
  }
  else{
    PosX++; // CW
  }
  SysTick_Wait(delay);
}
// Move 1.8 degrees counterclockwise, delay is wait after each step
void StepperX_CCW(uint32_t delay){
  PtX = PtX->Next[counterclockwise]; // circular
  STEPPERX = PtX->Out; // step motor
  if(PosX==0){        // shaft angle
    PosX = 199;       // reset
  }
  else{
    PosX--; // CCW
  }
  SysTick_Wait(delay); // blind-cycle wait
}
void StepperY_CW(uint32_t delay){
  PtY = PtY->Next[clockwise];     // circular
	STEPPERY = PtY->Out; // step motor
  if(PosY==199){      // shaft angle
    PosY = 0;         // reset
  }
  else{
    PosY++; // CW
  }
  SysTick_Wait(delay);
}
void StepperY_CCW(uint32_t delay){
  PtY = PtY->Next[counterclockwise]; // circular
  STEPPERY = PtY->Out; // step motor
  if(PosY==0){        // shaft angle
    PosY = 199;       // reset
  }
  else{
    PosY--; // CCW
  }
  SysTick_Wait(delay); // blind-cycle wait
}

// Initialize Stepper interface
void Stepper_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x18; // 1) activate port D,E
	while((SYSCTL_PRGPIO_R & 0x00000018) == 0){};
  SysTick_Init();
  PosX = 0;
	PosY = 0;
  PtX = &fsm[0]; 
	PtY = &fsm[0];
                                    // 2) no need to unlock PD3-0
//  GPIO_PORTD_AMSEL_R &= ~0x0F;      // 3) disable analog functionality on PD3-0
//  GPIO_PORTD_PCTL_R &= ~0x0000FFFF; // 4) GPIO configure PD3-0 as GPIO
// PortD initialization
  GPIO_PORTD_DIR_R |= 0x0F;   // 5) make PD3-0 out
  GPIO_PORTD_AFSEL_R &= ~0x0F;// 6) disable alt funct on PD3-0
  GPIO_PORTD_DR8R_R |= 0x0F;  // enable 8 mA drive
  GPIO_PORTD_DEN_R |= 0x0F;   // 7) enable digital I/O on PD3-0 
	
// PortE initialization	
	GPIO_PORTE_DIR_R |= 0x1F;   // 5) make PD3-0 out
  GPIO_PORTE_AFSEL_R &= ~0x1F;// 6) disable alt funct on PD3-0
  GPIO_PORTE_DR8R_R |= 0x1F;  // enable 8 mA drive
  GPIO_PORTE_DEN_R |= 0x1F;   // 7) enable digital I/O on PD3-0 
	
}

// Turn stepper motor to desired position
// (0 <= desired <= 199)
// time is the number of bus cycles to wait after each step
void Stepper_Seek(uint8_t desired, uint32_t time){
short CWsteps;
  if((CWsteps = (desired-PosX))<0){
    CWsteps+=200;
  } // CW steps is 0 to 199
  if(CWsteps > 100){
    while(desired != PosX){
      StepperX_CCW(time);
    }
  }
  else{
    while(desired != PosX){
      StepperX_CW(time);
    }
  }
}

