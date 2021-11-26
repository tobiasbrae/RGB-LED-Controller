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
#include <stdio.h>
#include <avr/interrupt.h>

#include "bitOperation.h"
#include "charBuffer.h"

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

#define DDR_RED &DDRD,5
#define DDR_GREEN &DDRD,6
#define DDR_BLUE &DDRD,7

#define OUT_RED &PORTD,5
#define OUT_GREEN &PORTD,6
#define OUT_BLUE &PORTD,7

#define RED_ON PORTD|=1<<5
#define RED_OFF PORTD&=~(1<<5)
#define GREEN_ON PORTD|=1<<6
#define GREEN_OFF PORTD&=~(1<<6)
#define BLUE_ON PORTD|=1<<7
#define BLUE_OFF PORTD&=~(1<<7)

// ==================================== [defines] ==========================================

#define UART_RX_SIZE 100
#define UART_TX_SIZE 100

#define CYCLE 2000

#define NUM_PARAM 5
#define PARAM_RED 0
#define PARAM_GREEN 1
#define PARAM_BLUE 2
#define PARAM_POWER 3
#define PARAM_AUTO 4

// ==================================== [variables] ==========================================

cb_charBuffer rxBuf, txBuf; // structures for the UART buffers
char rxData[UART_RX_SIZE]; // data for UART rx buffer
char txData[UART_TX_SIZE]; // data for UART tx buffer
volatile uint8_t isSending; // states if the transmitter is currently busy

uint8_t params[NUM_PARAM]; // stores all parameters

volatile uint16_t clock; // base clock [ms]
uint16_t cycleValue;
volatile uint8_t mode; // current color mode
volatile uint8_t pwmCycle; // used to track the pwm cycle

// ==================================== [function declaration] ==========================================

void initialize(void); // setting the timers, uart, etc.
void handleData(void); // check input buffer for possible commands

void sendChar(char data); // send a char via UART
void sendString(char *data); // send a string via UART

// ==================================== [program start] ==========================================

int main(void)
{
	initialize();

	cycleValue = CYCLE;

	while(1)
	{
		if(clock > cycleValue)
		{
			clock = 0;
			handleData();
		}
	}
}

void initialize(void)
{
	cli(); // disable global interrupts

	// PWM output configuration
	setBit(DDR_RED, 1); // set red pin as output
	setBit(DDR_GREEN, 1); // set green pin as output
	setBit(DDR_BLUE, 1); // set blue pin as output
	setBit(OUT_RED, 0); // switch red pin off
	setBit(OUT_GREEN, 0); // switch green pin off
	setBit(OUT_BLUE, 0); // switch blue pin off

	// pwm timer setup
	OCR1A = 25; // pwm interrupt frequency 20 kHz
	setBit(&TCCR1B, WGM13, 0); // ctc mode
	setBit(&TCCR1B, WGM12, 1);
	setBit(&TCCR1A, WGM11, 0);
	setBit(&TCCR1A, WGM10, 0);
	setBit(&TCCR1B, CS12, 0); // divider 8 -> 1 MHz
	setBit(&TCCR1B, CS11, 1);
	setBit(&TCCR1B, CS10, 0);
	setBit(&TIMSK, OCIE1A, 1); // enable output compare match interrupt

	// clock timer setup
	OCR2 = 32; // clock frequency 0.977 kHz -> clock tick 1.024 ms 
	setBit(&TCCR2, WGM21, 1); // ctc mode
	setBit(&TCCR2, WGM20, 0);
	setBit(&TCCR2, CS22, 1); // divider 256 -> 31.25 kHz
	setBit(&TCCR2, CS21, 1);
	setBit(&TCCR2, CS20, 0);
	setBit(&TIMSK, OCIE2, 1); // enable output compare match interrupt

	// UART setup
	UBRRL = 25; // set baud rate to 19.200
	setBit(&UCSRA, U2X, 0); // disable double data rate
	setBit(&UCSRB, RXCIE, 1); // enable rx complete interrupt
	setBit(&UCSRB, TXCIE, 1); // enable tx complete interrupt
	setBit(&UCSRB, RXEN, 1); // enable receiver
	setBit(&UCSRB, TXEN, 1); // enable transmitter
	setBit(&UCSRB, UCSZ2, 0); // set to 8 data bits
	UCSRC = (1 << URSEL & 1 << UCSZ1 & 1 << UCSZ0);

	cb_initBuffer(&rxBuf, rxData, UART_RX_SIZE);
	cb_initBuffer(&txBuf, txData, UART_TX_SIZE);

	sei(); // enable global interrupts
}

void switchMode(void)
{
	setBit(OUT_RED, 0); // switch red pin off
	setBit(OUT_GREEN, 0); // switch green pin off
	setBit(OUT_BLUE, 0); // switch blue pin off

	mode = (mode+1)%6;
	switch(mode)
	{
		case 0:
			setBit(OUT_RED, 1); // switch red pin on
			break;
		case 1:
			setBit(OUT_RED, 1); // switch red pin on
			setBit(OUT_GREEN, 1); // switch red pin on
			break;
		case 2:
			setBit(OUT_GREEN, 1); // switch green pin on
			break;
		case 3:
			setBit(OUT_GREEN, 1); // switch red pin on
			setBit(OUT_BLUE, 1); // switch blue pin on
			break;
		case 4:
			setBit(OUT_BLUE, 1); // switch green pin on
			break;
		case 5:
			setBit(OUT_BLUE, 1); // switch red pin on
			setBit(OUT_RED, 1); // switch blue pin on
			break;
		default:
			break;
	}
}

void handleData(void)
{
	uint8_t found = 0; // states if a command was found (\r)
	uint8_t length = cb_hasNext(&rxBuf); // current used length of rx buffer
	char buffer[20];
	for (uint8_t i = 0; i < length; i++)
		if(cb_getNextOff(&rxBuf, i) == '\r')
			{
				length = i + 1; // save length of given command
				found = 1;
				cb_getNextN(&rxBuf, buffer, length);
				break; // exit loop
			}
	if(found)
	{
		if(cb_getNext(&rxBuf) == 'r')
		{
			uint16_t value;
			if(sscanf(buffer, "r%u\r",&value) == 1 && value <= 255)
			{
				params[PARAM_RED] = (uint8_t) value;
				sendString("Static value red changed successfully.\r\n");
			}
			else
				sendString("Error. Usage: \"r<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'g')
		{
			uint16_t value;
			if(sscanf(buffer, "g%u\r",&value) == 1 && value <= 255)
			{
				params[PARAM_GREEN] = (uint8_t) value;
				sendString("Static value green changed successfully.\r\n");
			}
			else
				sendString("Error. Usage: \"g<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'b')
		{
			uint16_t value;
			if(sscanf(buffer, "b%u\r",&value) == 1 && value <= 255)
			{
				params[PARAM_BLUE] = (uint8_t) value;
				sendString("Static value blue changed successfully.\r\n");
			}
			else
				sendString("Error. Usage: \"b<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'p')
		{
			char value = cb_getNextOff(&rxBuf, 1);
			if(length == 3 && (value == '1' || value == '0'))
			{
				if(value == '1')
				{
					params[PARAM_POWER] = 1;
					sendString("Power enabled.\r\n");
				}
				else
				{
					params[PARAM_POWER] = 0;
					sendString("Power disabled.\r\n");
				}
			}
			else
			{
				sendString("Error. Usage: \"p<1/0>\"\r\n");
			}
		}
		else if(cb_getNext(&rxBuf) == 'a')
		{
			char value = cb_getNextOff(&rxBuf, 1);
			if(length == 3 && (value == '1' || value == '0'))
			{
				if(value == '1')
				{
					params[PARAM_AUTO] = 1;
					sendString("Auto-On enabled.\r\n");
				}
				else
				{
					params[PARAM_AUTO] = 0;
					sendString("Auto-On disabled.\r\n");
				}
			}
			else
			{
				sendString("Error. Usage: \"a<1/0>\"\r\n");
			}
		}
		else if(cb_getNext(&rxBuf) == 's')
		{
			sendString("save\r\n");
		}
		else
		{
			sendString("Unknown command!\r\n");
		}
		cb_deleteN(&rxBuf, length);
	}
}

void sendChar(char data)
{
	cb_put(&txBuf, data);
	if(!isSending)
	{
		isSending = 1;
		UDR = data;
	}
}

void sendString(char *data)
{
	cb_putString(&txBuf, data);
	if(!isSending)
	{
		isSending = 1;
		UDR = data[0];
	}
}

ISR(TIMER1_COMPA_vect) // PWM
{
	pwmCycle++;
	if(pwmCycle)
	{
		if(pwmCycle >= params[PARAM_RED])
			RED_OFF;
		if(pwmCycle >= params[PARAM_GREEN])
			GREEN_OFF;
		if(pwmCycle >= params[PARAM_BLUE])
			BLUE_OFF;
	}
	else if(params[PARAM_POWER])
	{
		if(params[PARAM_RED])
			RED_ON;
		if(params[PARAM_GREEN])
			GREEN_ON;
		if(params[PARAM_BLUE])
			BLUE_ON;
	}
}

ISR(TIMER2_COMP_vect) // clock tick
{
	clock++;
}

ISR(USART_RXC_vect) // UART receive complete
{
	char data = UDR;
	cb_put(&rxBuf, data);
	if(data == '\r')
		sendChar('\n');
	sendChar(data);
}

ISR(USART_TXC_vect) // UART transmit complete
{
	cb_delete(&txBuf);
	if(cb_getNext(&txBuf))
		UDR = cb_getNext(&txBuf);
	else
		isSending = 0;
}