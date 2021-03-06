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

#define UART_RX_SIZE 50
#define UART_TX_SIZE 100

#define NUM_PARAM 6
#define PARAM_CHECK_EEPROM 0
#define PARAM_RED 1
#define PARAM_GREEN 2
#define PARAM_BLUE 3
#define PARAM_POWER 4
#define PARAM_AUTO 5

#define CHECK_EEPROM 0xAA

// ==================================== [variables] ==========================================

cb_charBuffer rxBuf, txBuf; // structures for the UART buffers
char rxData[UART_RX_SIZE]; // data for UART rx buffer
char txData[UART_TX_SIZE]; // data for UART tx buffer
volatile uint8_t isSending; // states if the transmitter is currently busy

uint8_t params[NUM_PARAM]; // stores all parameters

volatile uint16_t clock; // base clock [ms]
volatile uint8_t pwmCycle; // used to track the pwm cycle

// ==================================== [function declaration] ==========================================

void initialize(void); // setting the timers, uart, etc.
void handleData(void); // check input buffer for possible commands
void loadParams(void); // load parameters from eeprom
void storeParams(void); // store parameters to eeprom

// ==================================== [program start] ==========================================

int main(void)
{
	initialize();
	loadParams();

	if(params[PARAM_AUTO])
		params[PARAM_POWER] = 1;
	else
		params[PARAM_POWER] = 0;

	while(1)
	{
		if(clock > 5 && cb_hasNext(&txBuf))
		{
			UDR = cb_getNext(&txBuf);
			cb_delete(&txBuf);
			clock = 0;
		}
		handleData();
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
				cb_putString(&txBuf, "Static value red changed successfully.\r\n");
			}
			else
				cb_putString(&txBuf, "Error. Usage: \"r<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'g')
		{
			uint16_t value;
			if(sscanf(buffer, "g%u\r",&value) == 1 && value <= 255)
			{
				params[PARAM_GREEN] = (uint8_t) value;
				cb_putString(&txBuf, "Static value green changed successfully.\r\n");
			}
			else
				cb_putString(&txBuf, "Error. Usage: \"g<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'b')
		{
			uint16_t value;
			if(sscanf(buffer, "b%u\r",&value) == 1 && value <= 255)
			{
				params[PARAM_BLUE] = (uint8_t) value;
				cb_putString(&txBuf, "Static value blue changed successfully.\r\n");
			}
			else
				cb_putString(&txBuf, "Error. Usage: \"b<0...255>\"\r\n");
		}
		else if(cb_getNext(&rxBuf) == 'p')
		{
			char value = cb_getNextOff(&rxBuf, 1);
			if(length == 3 && (value == '1' || value == '0'))
			{
				if(value == '1')
				{
					params[PARAM_POWER] = 1;
					cb_putString(&txBuf, "Power enabled.\r\n");
				}
				else
				{
					params[PARAM_POWER] = 0;
					cb_putString(&txBuf, "Power disabled.\r\n");
				}
			}
			else
			{
				cb_putString(&txBuf, "Error. Usage: \"p<1/0>\"\r\n");
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
					cb_putString(&txBuf, "Auto-On enabled.\r\n");
				}
				else
				{
					params[PARAM_AUTO] = 0;
					cb_putString(&txBuf, "Auto-On disabled.\r\n");
				}
			}
			else
			{
				cb_putString(&txBuf, "Error. Usage: \"a<1/0>\"\r\n");
			}
		}
		else if(cb_getNext(&rxBuf) == 's')
		{
			if(length == 3 && cb_getNextOff(&rxBuf, 1) == 'y')
			{
				storeParams();
				cb_putString(&txBuf, "Parameters stored.\r\n");
			}
			else
			{
				cb_putString(&txBuf, "Error. Usage: \"sy\"\r\n");
			}
		}
		else if(cb_getNext(&rxBuf) == 'l')
		{
			if(length == 3 && cb_getNextOff(&rxBuf, 1) == 'y')
			{
				loadParams();
				cb_putString(&txBuf, "Parameters loaded.\r\n");
			}
			else
			{
				cb_putString(&txBuf, "Error. Usage: \"ly\"\r\n");
			}
		}
		else
		{
			cb_putString(&txBuf, "Unknown command!\r\n");
		}
		cb_deleteN(&rxBuf, length);
	}
}

void loadParams(void)
{
	uint8_t abort = 0; // states if the load operation was aborted
	cli();
	for(uint8_t i = 0; i < NUM_PARAM; i++)
	{
		while(EECR & (1 << EEWE)); // wait for write to finish
		EEARH = 0;
		EEARL = i;
		EECR |= 1 << EERE;
		params[i] = EEDR;
		if(i == PARAM_CHECK_EEPROM && params[i] != CHECK_EEPROM)
			abort = 1;
		if(abort)
			params[i] = 0;
	}
	sei();
}

void storeParams(void)
{
	cli();
	params[PARAM_CHECK_EEPROM] = CHECK_EEPROM; // store the eeprom check value in the parameters
	for(uint8_t i = 0; i < NUM_PARAM; i++) // loop through all parameters
	{
		while(EECR & (1 << EEWE)); // wait for previous write to finish
		EEARH = 0;
		EEARL = i; // write address
		EEDR = params[i]; // write parameter
		EECR = 1 << EEMWE; // set master write enable bit
		EECR |= 1 << EEWE; // set write enable bit
	}
	sei();
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
		cb_put(&txBuf, '\n');
	cb_put(&txBuf, data);
}

ISR(USART_TXC_vect) // UART transmit complete
{
	// nothing to do here
}