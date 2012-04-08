
/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Philip Heron <phil@sanslogic.co.uk>               */
/*                     Nigel Smart <nigel@projectswift.co.uk>            */
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
#include <avr/interrupt.h>
#include <util/crc16.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "ax25modem.h"

static const uint8_t const _sine_table[] = {
#include "sine_table.h"
};

#define TXPIN    (1 << 7) /* PD7 */
#define TXENABLE (1 << 4) /* PA4 */

#define BAUD_RATE   (1200)
#define TABLE_SIZE  (512)
#define REST_BYTES  (10)

#define PLAYBACK_RATE    (F_CPU / 256)
#define SAMPLES_PER_BAUD (PLAYBACK_RATE / BAUD_RATE)
#define PHASE_DELTA_1200 (((TABLE_SIZE * 1200L) << 7) / PLAYBACK_RATE)
#define PHASE_DELTA_2200 (((TABLE_SIZE * 2200L) << 7) / PLAYBACK_RATE)
#define PHASE_DELTA_XOR  (PHASE_DELTA_1200 ^ PHASE_DELTA_2200)

/* Data to be transmitted */
volatile static uint8_t *_txbuf = 0;
volatile static uint8_t  _txlen = 0;

ISR(TIMER2_OVF_vect)
{
	static uint16_t phase  = 0;
	static uint16_t step   = PHASE_DELTA_1200;
	static uint16_t sample = 0;
	static uint8_t rest    = REST_BYTES * 2;
	static uint8_t byte;
	static uint8_t bit     = 7;
	static int8_t bc       = 0;
	
	/* Update the PWM output */
	OCR2A = _sine_table[(phase >> 7) & 0x1FF];
	phase += step;
	
	if(++sample < SAMPLES_PER_BAUD) return;
	sample = 0;
	
	/* Zero-bit insertion */
	if(bc == 5)
	{
		step ^= PHASE_DELTA_XOR;
		bc = 0;
		return;
	}
	
	/* Load the next byte */
	if(++bit == 8)
	{
		bit = 0;
		
		if(rest > REST_BYTES || !_txlen)
		{
			if(!--rest)
			{
				/* Disable radio and interrupt */
				PORTA &= ~TXENABLE;
				TIMSK2 &= ~_BV(TOIE2);
				
				/* Prepare state for next run */
				phase = sample = 0;
				step  = PHASE_DELTA_1200;
				rest  = REST_BYTES * 2;
				bit   = 7;
				bc    = 0;
				
				return;
			}
			
			/* Rest period, transmit ax.25 header */
			byte = 0x7E;
			bc = -1;
		}
		else
		{
			/* Read the next byte from memory */
			byte = *(_txbuf++);
			if(!--_txlen) rest = REST_BYTES + 2;
			if(bc < 0) bc = 0;
		}
	}
	
	/* Find the next bit */
	if(byte & 1)
	{
		/* 1: Output frequency stays the same */
		if(bc >= 0) bc++;
	}
	else
	{
		/* 0: Toggle the output frequency */
		step ^= PHASE_DELTA_XOR;
		if(bc >= 0) bc = 0;
	}
	
	byte >>= 1;
}

void ax25_init(void)
{
	/* Fast PWM mode, non-inverting output on OC2A */
	TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
	TCCR2B = _BV(CS20);
	
	/* Make sure radio is not enabled */
	PORTA &= ~TXENABLE;
	
	/* Enable pins for output (Port A pin 4 and Port D pin 7) */
	DDRA |= TXENABLE;
	DDRD |= TXPIN;
}

static uint16_t _update_crc(uint16_t x, uint8_t byte)
{
	char i;
	for(i = 0; i < 8; i++)
	{
		x ^= (byte >> i) & 1;
		if(x & 1) x = (x >> 1) ^ 0x8408; /* X-modem CRC poly */
		else x = x >> 1;
	}
	
	return(x);
}

static uint8_t *_ax25_callsign(uint8_t *s, char *callsign, char ssid)
{
	char i;
	for(i = 0; i < 6; i++)
	{
		if(*callsign) *(s++) = *(callsign++) << 1;
		else *(s++) = ' ' << 1;
	}
	*(s++) = ('0' + ssid) << 1;
	return(s);
}

void ax25_frame(char *scallsign, char sssid, char *dcallsign, char dssid,
		char *path1, char ttl1, char *path2, char ttl2, char *data, ...)
{
	uint8_t frame[100];
	uint8_t *s;
	uint16_t x;
	va_list va;
	
	va_start(va, data);
	
	/* Pause while there is still data transmitting */
	while(_txlen);
	
	/* Write in the callsigns and paths */
	s = _ax25_callsign(frame, dcallsign, dssid);
	s = _ax25_callsign(s, scallsign, sssid);
	if(path1) s = _ax25_callsign(s, path1, ttl1);
	if(path2) s = _ax25_callsign(s, path2, ttl2);
	
	/* Mark the end of the callsigns */
	s[-1] |= 1;
	
	*(s++) = 0x03; /* Control, 0x03 = APRS-UI frame */
	*(s++) = 0xF0; /* Protocol ID: 0xF0 = no layer 3 data */
	
	vsnprintf((char *) s, 100 - (s - frame) - 2, data, va);
	va_end(va);
	
	/* Calculate and append the checksum */
	for(x = 0xFFFF, s = frame; *s; s++)
		x = _update_crc(x, *s);
	
	*(s++) = ~(x & 0xFF);
	*(s++) = ~((x >> 8) & 0xFF);
	
	/* Point the interrupt at the data to be transmit */
	_txbuf = frame;
	_txlen = s - frame;
	
	/* Enable the timer and key the radio */
	TIMSK2 |= _BV(TOIE2);
	PORTA |= TXENABLE;
}

char *ax25_base91enc(char *s, uint8_t n, uint32_t v)
{
	/* Creates a Base-91 representation of the value in v in the string */
	/* pointed to by s, n-characters long. String length should be n+1. */
	
	for(s += n, *s = '\0'; n; n--)
	{
		*(--s) = v % 91 + 33;
		v /= 91;
	}
	
	return(s);
}

