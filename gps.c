
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
#include <stdbool.h>
#include "gps.h"

void gps_setup(void)

{

        /* Do UART1 initialisation, 9600 baud */
        UBRR1H = 0;
        UBRR1L = F_CPU / 16 / 9600 - 1;

        /* Enable RX, TX and RX interrupt */
        UCSR1B = (1 << RXEN1) | (1 << TXEN1);

        /* 8-bit, no parity and 1 stop bit */
        UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

bool gps_get_pos(int32_t* lat, int32_t* lon, int32_t* alt)

{
	uint8_t request[8] = {0xB5, 0x62, 0x01, 0x02, 0x00, 0x00, 0x03, 0x0A};
	uint8_t buf[36];
	uint8_t i;

	_gps_send_msg(request, 8);

	for(i = 0; i < 36; i++)
	buf[i] = _gps_get_byte();

	if( buf[0] != 0xB5 || buf[1] != 0x62 )
		return false;

	if( buf[2] != 0x01 || buf[3] != 0x02 )
		return false;

        if( !_gps_verify_checksum(&buf[2], 32) )
                return false;

	*lon = (int32_t)buf[10] | (int32_t)buf[11] << 8 |
		(int32_t)buf[12] << 16 | (int32_t)buf[13] << 24;

	*lat = (int32_t)buf[14] | (int32_t)buf[15] << 8 |
		(int32_t)buf[16] << 16 | (int32_t)buf[17] << 24;

	*alt = (int32_t)buf[22] | (int32_t)buf[23] << 8 |
		(int32_t)buf[24] << 16 | (int32_t)buf[25] << 24;

        return true;
}

bool _gps_verify_checksum(uint8_t* data, uint8_t len)

{
	uint8_t a, b;
	gps_ubx_checksum(data, len, &a, &b);
	if( a != *(data + len) || b != *(data + len + 1))
		return false;
	else
		return true;
}

void gps_ubx_checksum(uint8_t* data, uint8_t len, uint8_t* cka, uint8_t* ckb)

{
	uint8_t i;

	*cka = 0;
	*ckb = 0;

	for( i = 0; i < len; i++ )
	{
		*cka += *data;
		*ckb += *cka;
		data++;
	}
}

void _gps_send_msg(uint8_t* data, uint8_t len)

{
	uint8_t i;
	_gps_flush_buffer();
	for(i = 0; i < len; i++)
	{
		while( !(UCSR1A & (1<<UDRE1)) );
		UDR1 = *data;
		data++;
	}
	while( !(UCSR1A & (1<<UDRE1)) );

}

uint8_t _gps_get_byte(void)

{
	while( !(UCSR1A & _BV(RXC1)) );
	return UDR1;

}

void _gps_flush_buffer(void)

{
	uint8_t dummy;
	while ( UCSR1A & _BV(RXC1) ) dummy = UDR1;

}
