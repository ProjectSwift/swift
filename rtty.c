
/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Philip Heron <phil@sanslogic.co.uk>               */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "rtty.h"
#include "timeout.h"

/* MARK = Upper tone, Idle, bit  */
#define TXSPACE  (1 << 0) /* PA0 */
#define TXMARK   (1 << 1) /* PA1 */
#define TXENABLE (1 << 2) /* PA2 */

#define TXBIT(b) PORTA = (PORTA & ~(TXMARK | TXSPACE)) | ((b) ? TXMARK : TXSPACE)

volatile static uint8_t  txpgm = 0;
volatile static uint8_t *txbuf = 0;
volatile static uint16_t txlen = 0;

ISR(TIMER0_COMPA_vect)
{
	/* The currently transmitting byte, including framing */
	static uint8_t byte = 0x00;
	static uint8_t bit  = 0x00;
	
	uint8_t b = 0;
	switch(bit++)
	{
	case 0: b = 0; break; /* Start bit */
	case 9: b = 1; break; /* Stop bit */
	//case 10: b = 1; TCNT0 += OCR0A >> 1; bit = 0; break; /* Stop bit 0.5 */
	case 10: b = 1; bit = 0; break; /* Stop bit part 2 */
	default: b = byte & 1; byte >>= 1; break;
	}
	
	TXBIT(b);
	
	if(bit == 0 && txlen > 0)
	{
		if(txpgm == 0) byte = *(txbuf++);
		else byte = pgm_read_byte(txbuf++);
		txlen--;
	}
	
	/* Timeout tick */
	to_tick(RTTY_BAUD);
}

void rtx_enable(char en)
{
	if(en) PORTA |= TXENABLE;
	else PORTA &= ~TXENABLE;
}

void rtx_init(void)
{
	/* RTTY is driven by TIMER0 in CTC mode */
	TCCR0A = _BV(WGM01); /* Mode 2, CTC */
	TCCR0B = _BV(CS02) | _BV(CS00); /* prescaler 1024 */
	OCR0A = F_CPU / 1024 / RTTY_BAUD - 1;
	TIMSK0 = _BV(OCIE0A); /* Enable interrupt */
	
	/* We use Port B pins 1 and 2 */
	TXBIT(1);
	rtx_enable(0);
	DDRA |= TXMARK | TXSPACE | TXENABLE;
}

void inline rtx_wait(void)
{
	/* Wait for interrupt driven TX to finish */
	while(txlen > 0) while(txlen > 0);
}

void rtx_data(uint8_t *data, size_t length)
{
	rtx_wait();
	txpgm = 0;
	txbuf = data;
	txlen = length;
}

void rtx_data_P(PGM_P data, size_t length)
{
	rtx_wait();
	txpgm = 1;
	txbuf = (uint8_t *) data;
	txlen = length;
}

void rtx_string(char *s)
{
	uint16_t length = strlen(s);
	rtx_data((uint8_t *) s, length);
}

void rtx_string_P(PGM_P s)
{
	uint16_t length = strlen_P(s);
	rtx_data_P(s, length);
}

