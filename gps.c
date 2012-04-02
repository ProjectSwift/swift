
/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include "gps.h"

#define UBX_INT32(buf, offset) (int32_t) buf[offset] | (int32_t) buf[offset + 1] << 8 | (int32_t) buf[offset + 2] << 16 | (int32_t) buf[offset + 3] << 24

static uint8_t _gps_get_byte(void)
{
	while(!(UCSR1A & _BV(RXC1)));
	return(UDR1);
}

static void _gps_send_byte(uint8_t b, uint8_t *ck_a, uint8_t *ck_b)
{
	while(!(UCSR1A & _BV(UDRE1)));
	UDR1 = b;
	if(ck_a && ck_b) *ck_b += *ck_a += b;
}

static void _gps_send_packet(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length)
{
	uint8_t ck_a = 0;
	uint8_t ck_b = 0;
	
	_gps_send_byte(0xB5, 0, 0);
	_gps_send_byte(0x62, 0, 0);
	
	_gps_send_byte(class,         &ck_a, &ck_b);
	_gps_send_byte(id,            &ck_a, &ck_b);
	_gps_send_byte(length & 0xFF, &ck_a, &ck_b);
	_gps_send_byte(length >> 8,   &ck_a, &ck_b);
	
	while(length--) _gps_send_byte(*(payload++), &ck_a, &ck_b);
	
	_gps_send_byte(ck_a, 0, 0);
	_gps_send_byte(ck_b, 0, 0);
}

static bool _gps_get_packet(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length)
{
	int8_t h = -1;
	uint8_t b, *p = 0;
	uint8_t ck_a = 0, ck_b = 0;
	uint16_t len = 0, i;
	
	/* Search for the start of the expected packet */
	for(i = 0; i < 4000 + len; i++)
	{
		b = _gps_get_byte();
		
		if(h > 1 && h < 7) ck_b += ck_a += b;
		
		switch(h)
		{
		case -1: len = ck_a = ck_b = 0; p = payload; h++;
		case 0: if(b == 0xB5)  h++; break;
		case 1: if(b == 0x62)  h++; else h = -1; break;
		case 2: if(b == class) h++; else h = -1; break;
		case 3: if(b == id)    h++; else h = -1; break;
		case 4: len  = b; h++; break;
		case 5: len += b << 8; h++;
			if(len != length) return(false);
			if(len == 0) h++;
			break;
		case 6: *(p++) = b; if(--len) break; else h++; break;
		case 7: if(b == ck_a)  h++; else return(false); break;
		case 8: if(b == ck_b)  h++; else return(false); break;
		}
		
		/* 9 == success */
		if(h == 9) break;
	}
	
	return(h == 9);
}

void _gps_setup_port(void)
{
	/* This command configures the module for 9600 baud, 8n1,
	 * and disables NMEA output and input */
	uint8_t cmd[20] = { 0x01, 0x00, 0x00, 0x00, 0xC0, 0x08, 0x00, 0x00, 0x80, 0x25, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
	
	_gps_send_packet(0x06, 0x00, cmd, 20);
	
	/* TODO: Test for ACK */
}

void gps_setup(void)
{
	/* Do UART1 initialisation, 9600 baud */
	UBRR1H = (F_CPU / 16 / 9600 - 1) >> 8;
	UBRR1L = F_CPU / 16 / 9600 - 1;
	
	/* Enable RX and TX */
	UCSR1B = _BV(RXEN1) | _BV(TXEN1);
	
	/* 8-bit, no parity and 1 stop bit */
	UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
	
	/* Configure the GPS module */
	_gps_setup_port();
}

bool gps_get_pos(int32_t* lat, int32_t* lon, int32_t* alt)
{
	uint8_t buf[28];
	
	_gps_send_packet(0x01, 0x02, 0, 0);
	if(!_gps_get_packet(0x01, 0x02, buf, 28)) return(false);
	
	*lon = UBX_INT32(buf, 4);
	*lat = UBX_INT32(buf, 8);
	*alt = UBX_INT32(buf, 16);
	
	return(true);
}

bool gps_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second)
{
	uint8_t buf[20];

	_gps_send_packet(0x01, 0x21, 0, 0);
	if(!_gps_get_packet(0x01, 0x21, buf, 20)) return(false);
	
	*hour = buf[16];
	*minute = buf[17];
	*second = buf[18];
	
	return(true);
}

bool gps_check_lock(uint8_t *lock, uint8_t *sats)
{
	uint8_t buf[52];
	
	_gps_send_packet(0x01, 0x06, 0, 0);
	if(!_gps_get_packet(0x01, 0x06, buf, 52)) return(false);
	
	*lock = (buf[11] & 0x01 ? buf[10] : 0);
	*sats = buf[47];
	
	return(true);
}

uint8_t gps_check_nav(void)
{
	uint8_t buf[36];
	
	_gps_send_packet(0x06, 0x24, 0, 0);
	if(!_gps_get_packet(0x06, 0x24, buf, 36)) return(0);
	
	return(buf[2]);
}

