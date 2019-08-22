// Stepper.h
// Runs on LM4F120/TM4C123
// Provide functions that step the motor once clockwise, step
// once counterclockwise, and initialize the stepper motor
// interface.

// PD3 connected to driver for stepper motor coil A
// PD2 connected to driver for stepper motor coil A'
// PD1 connected to driver for stepper motor coil B
// PD0 connected to driver for stepper motor coil B'


// Initialize Stepper interface
void Stepper_Init(void);

// Move 1.8 degrees clockwise, 
// delay is the time to wait after each step in bus cycles
void StepperX_CW(uint32_t delay);
void StepperY_CW(uint32_t delay);

// Move 1.8 degrees counterclockwise,
// delay is the time to wait after each step in bus cycles
void StepperX_CCW(uint32_t delay);
void StepperY_CCW(uint32_t delay);

// Turn stepper motor to desired position
// (0 <= desired <= 199)
// time is the number of bus cycles to wait after each step
void Stepper_Seek(uint8_t desired, uint32_t time);

