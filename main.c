// ====================== [main.c (RGB-LED-Controller)] ==========================================
/*
*	A programm to control RGB-LED-Strips with an Atmel ATmega8 running at 8 MHz.
*	Controlled via UART (see README.md)
*
*	Author: Tobias Braechter
*
*/

// ==================================== [includes] =========================================

#include <avr/io.h>
#include <stdint.h>
#include <avr/interrupt.h>

#include "bitOperation.h"

// ==================================== [pin configuration] ===============================

// PORTB0 (ICP1)		- Unused
// PORTB1 (OC1A)		- Unused
// PORTB2 (SS/OC1B))	- Unused
// PORTB3 (MOSI/OC2)	- Unused
// PORTB4 (MISO)		- Unused
// PORTB5 (SCK)			- Unused
// PORTB6 (XTAL1)		- Unused
// PORTB7 (XTAL2)		- Unused

// PORTC0 (ADC0)		- Unused
// PORTC1 (ADC1)		- Unused
// PORTC2 (ADC2)		- Unused
// PORTC3 (ADC3)		- Unused
// PORTC4 (ADC4/SDA)	- Unused
// PORTC5 (ADC5/SCL)	- Unused
// PORTC6 (Reset)		- Unused

// PORTD0 (RXD)			- UART
// PORTD1 (TXD)			- UART
// PORTD2 (INT0)		- Unused
// PORTD3 (INT1)		- Unused
// PORTD4 (XCK/T0)		- PWM output red
// PORTD5 (T1)			- PWM output green
// PORTD6 (AIN0)		- PWM output blue
// PORTD7 (AIN1)		- Unused

#define DDR_PWM_R &DDRD,5
#define DDR_PWM_G &DDRD,6
#define DDR_PWM_B &DDRD,7

#define PWM_R &PORTD,5
#define PWM_G &PORTD,6
#define PWM_B &PORTD,7

#define CYCLE 2000

// ==================================== [variables] ==========================================

volatile uint16_t clock; // base clock [ms]
volatile uint8_t mode; // current color mode

// ==================================== [function declaration] ==========================================

void initialize(void); // setting the timers, uart, etc.
void switchMode(void); // switch the color mode

// ==================================== [program start] ==========================================

int main(void)
{
	initialize();

	while(1)
	{
		if(clock > CYCLE)
		{
			clock = 0;
			switchMode();
		}
	}
}

void initialize(void)
{
	cli(); // disable global interrupts

	mode = 0;

	// PWM output configuration
	setBit(DDR_PWM_R, 1); // set red pin as output
	setBit(DDR_PWM_G, 1); // set green pin as output
	setBit(DDR_PWM_B, 1); // set blue pin as output
	setBit(PWM_R, 0); // switch red pin off
	setBit(PWM_G, 0); // switch green pin off
	setBit(PWM_B, 0); // switch blue pin off

	// pwm timer setup
	OCR1A = 50; // pwm interrupt frequency 20 kHz
	setBit(&TCCR1B, WGM13, 0); // ctc mode
	setBit(&TCCR1B, WGM12, 1);
	setBit(&TCCR1A, WGM11, 0);
	setBit(&TCCR1A, WGM10, 0);
	setBit(&TCCR1B, CS12, 0); // divider 8 -> 1 MHz
	setBit(&TCCR1B, CS11, 1);
	setBit(&TCCR1B, CS10, 0);
	//setBit(&TIMSK, OCIE1A, 1); // enable output compare match interrupt

	// clock timer setup
	OCR2 = 32; // clock frequency 0.977 kHz -> clock tick 1.024 ms 
	setBit(&TCCR2, WGM21, 1); // ctc mode
	setBit(&TCCR2, WGM20, 0);
	setBit(&TCCR2, CS22, 1); // divider 256 -> 31.25 kHz
	setBit(&TCCR2, CS21, 1);
	setBit(&TCCR2, CS20, 0);
	setBit(&TIMSK, OCIE2, 1); // enable output compare match interrupt

	sei(); // enable global interrupts
}

void switchMode(void)
{
	setBit(PWM_R, 0); // switch red pin off
	setBit(PWM_G, 0); // switch green pin off
	setBit(PWM_B, 0); // switch blue pin off

	mode = (mode+1)%6;
	switch(mode)
	{
		case 0:
			setBit(PWM_R, 1); // switch red pin on
			break;
		case 1:
			setBit(PWM_R, 1); // switch red pin on
			setBit(PWM_G, 1); // switch red pin on
			break;
		case 2:
			setBit(PWM_G, 1); // switch green pin on
			break;
		case 3:
			setBit(PWM_G, 1); // switch red pin on
			setBit(PWM_B, 1); // switch blue pin on
			break;
		case 4:
			setBit(PWM_B, 1); // switch green pin on
			break;
		case 5:
			setBit(PWM_B, 1); // switch red pin on
			setBit(PWM_R, 1); // switch blue pin on
			break;
		default:
			break;
	}
}

ISR(TIMER1_COMPA_vect) // PWM
{
	/*
	pwmCycle++;
	if(!pwmCycle && pwmDuty) // cycle finished
		setBit(PWM, 1); // switch pin on
	if(pwmCycle >= pwmDuty)
		setBit(PWM,	0); // switch pin off
	*/
}

ISR(TIMER2_COMP_vect) // clock tick
{
	clock++;
}
