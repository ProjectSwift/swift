/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/* And Philip Heron <phil@sanslogic.co.uk>                               */
/* Additional Help from Jon Sowman & Adam Greig of Cambridge University  */
/* Spaceflight (www.cusf.co.uk)                                          */
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
#include <stdint.h>
#include <avr/io.h>
#include "gps.h"
#include "timeout.h"

/* UBX messages classes */
#define UBX_CLASS_NAV (0x01) /* Navigation Results */
#define UBX_CLASS_RXM (0x02) /* Receiver Manager Messages */
#define UBX_CLASS_INF (0x04) /* Information Messages */
#define UBX_CLASS_ACK (0x05) /* Ack/Nack Messages */
#define UBX_CLASS_CFG (0x06) /* Configuration Input Messages */
#define UBX_CLASS_MON (0x0A) /* Monitoring Messages */
#define UBX_CLASS_AID (0x0B) /* AssistNow Aiding Messages */
#define UBX_CLASS_TIM (0x0D) /* Timing Messages */

#define U4(buf, i) ( \
	(uint32_t) ((uint8_t) buf[i + 0]) | \
	(uint32_t) ((uint8_t) buf[i + 1]) << 8 | \
	(uint32_t) ((uint8_t) buf[i + 2]) << 16 | \
	(uint32_t) ((uint8_t) buf[i + 3]) << 24)
#define I4(buf, i) ((int32_t) U4(buf, i))

#define U2(buf, i) ( \
	(uint16_t) ((uint8_t) buf[i + 0]) | \
	(uint16_t) ((uint8_t) buf[i + 1]) << 8)
#define I2(buf, i) ((int16_t) U2(buf, i))

#define U1(buf, i) ((uint8_t) buf[i])
#define I1(buf, i) ((int8_t) buf[i])

#define MAX_PAYLOAD (64)
static uint8_t _buf[MAX_PAYLOAD];

static void _gps_flush(void)
{
	uint8_t b;
	while(UCSR1A & _BV(RXC1)) b = UDR1;
}

static uint8_t _gps_get_byte(uint8_t *byte, to_int ts, to_int timeout)
{
	if(!byte) return(GPS_ERROR);
	
	/* Wait for a byte */
	while(!(UCSR1A & _BV(RXC1)))
	{
		/* Test for timeout */
		if(to_since(ts) >= timeout) return(GPS_TIMEOUT);
	}
	
	/* Read received byte and return OK */
	*byte = UDR1;
	
	return(GPS_OK);
}

static void _gps_send_byte(uint8_t b, uint8_t *ck_a, uint8_t *ck_b)
{
	/* Wait for UART1 to free up and then transmit byte */
	while(!(UCSR1A & _BV(UDRE1)));
	UDR1 = b;
	
	/* Update checksum if required */
	if(ck_a && ck_b) *ck_b += *ck_a += b;
}

void gps_send_packet(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length)
{
	uint8_t ck_a = 0;
	uint8_t ck_b = 0;
	
	/* SYNC codes */
	_gps_send_byte(0xB5, 0, 0);
	_gps_send_byte(0x62, 0, 0);
	
	/* Header */
	_gps_send_byte(class,         &ck_a, &ck_b);
	_gps_send_byte(id,            &ck_a, &ck_b);
	_gps_send_byte(length & 0xFF, &ck_a, &ck_b);
	_gps_send_byte(length >> 8,   &ck_a, &ck_b);
	
	/* Payload */
	while(length--) _gps_send_byte(*(payload++), &ck_a, &ck_b);
	
	/* Checksum */
	_gps_send_byte(ck_a, 0, 0);
	_gps_send_byte(ck_b, 0, 0);
}

int gps_get_packet(uint8_t *class, uint8_t *id, uint8_t *payload, uint16_t *length, to_int timeout)
{
	int8_t s = -1;
	uint8_t b, ck_a, ck_b;
	uint16_t len;
	to_int timestamp;
	
	/* Flush the UART buffer */
	_gps_flush();
	
	/* Note the current time */
	timestamp = to_clock();
	
	/* Loop until timeout or packet received */
	len = ck_a = ck_b = 0;
	while(_gps_get_byte(&b, timestamp, timeout) == GPS_OK)
	{
		/* Update the CRC if reading the header or payload */
		if(s > 1 && s < 7) ck_b += ck_a += b;
		
		/* Parse the incoming data */
		switch(s)
		{
		case -1: len = ck_a = ck_b = 0; s++;
		case 0: if(b == 0xB5)  s++; break;
		case 1: if(b == 0x62)  s++; else s = -1; break;
		case 2: *class = b; s++; break;
		case 3: *id = b; s++; break;
		case 4: len = b; s++; break;
		case 5: len += b << 8; s += (len ? 1 : 2);
			if(len > *length) { *length = len; return(GPS_BUFFER_FULL); }
			*length = len;
			break;
		case 6: *(payload++) = b; if(--len) break; else s++; break;
		case 7: if(b == ck_a) s++; else return(GPS_BAD_CRC); break;
		case 8: if(b == ck_b) return(GPS_OK); else return(GPS_BAD_CRC);
		}
	}
	
	/* If we reach here, the timeout has triggered */
	return(GPS_TIMEOUT);
}

int gps_get_packet_type(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length, to_int timeout)
{
	uint16_t l;
	uint8_t c, i;
	int r;
	
	c = i = 0;
	
	/* Read the next packet */
	l = length;
	r = gps_get_packet(&c, &i, payload, &l, timeout);
	
	if(r != GPS_OK) return(r);
	
	/* Verify it's the correct type */
	if(c != class || i != id) return(GPS_UNEXPECTED);
	
	/* Verify the length */
	if(l != length) return(GPS_UNEXPECTED);
	
	/* Looks good */
	return(GPS_OK);
}

int gps_get_ack(uint8_t class, uint8_t id, to_int timeout)
{
	uint8_t payload[2];
	uint16_t l;
	uint8_t c, i;
	int r;
	
	/* Read the next packet */
	l = sizeof(payload);
	r = gps_get_packet(&c, &i, payload, &l, timeout);
	if(r != GPS_OK) return(r);
	
	/* Verify it's the correct type */
	if(c != UBX_CLASS_ACK) return(GPS_UNEXPECTED);
	if(i != 0x00 && i != 0x01) return(GPS_UNEXPECTED);
	if(l != 2) return(GPS_UNEXPECTED);
	
	/* Verify it's for the specified message */
	if(payload[0] != class || payload[1] != id) return(GPS_UNEXPECTED);
	
	/* Response is a valid ACK-NAK */
	if(i == 0x00) return(GPS_NAK);
	
	/* Response is a valid ACK-ACK */
	return(GPS_OK);
}

static int _gps_setup_port(void)
{
	/* This command configures the module for 9600 baud, 8n1,
	 * and disables NMEA output and input */
	uint8_t cmd[20] = { 0x01, 0x00, 0x00, 0x00, 0xC0, 0x08, 0x00, 0x00, 0x80, 0x25, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int r;
	
	gps_send_packet(UBX_CLASS_CFG, 0x00, cmd, 20);
	
	/* Check the response from the GPS module */
	r = gps_get_ack(UBX_CLASS_CFG, 0x00, 1500);
	
	return(r);
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

int gps_get_pos(int32_t *lat, int32_t *lon, int32_t *alt)
{
	int r;
	
	/* Transmit the request and read response */
	gps_send_packet(UBX_CLASS_NAV, 0x02, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_NAV, 0x02, _buf, 28, 1500);
	if(r != GPS_OK) return(r);
	
	/* Parse response */
	if(lon) *lon = I4(_buf, 4);
	if(lat) *lat = I4(_buf, 8);
	if(alt) *alt = I4(_buf, 16);
	
	return(GPS_OK);
}

int gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	int r;
	
	/* Transmit the request and read response */
	gps_send_packet(UBX_CLASS_NAV, 0x21, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_NAV, 0x21, _buf, 20, 1500);
	if(r != GPS_OK) return(r);
	
	/* Parse response */
	if(hour)   *hour   = U1(_buf, 16);
	if(minute) *minute = U1(_buf, 17);
	if(second) *second = U1(_buf, 18);
	
	return(GPS_OK);
}

int gps_get_lock(uint8_t *lock, uint32_t *pacc, uint16_t *pdop, uint8_t *sats)
{
	int r;
	
	/* Transmit the request and read response */
	gps_send_packet(UBX_CLASS_NAV, 0x06, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_NAV, 0x06, _buf, 52, 1500);
	if(r != GPS_OK) return(r);
	
	/* Parse response */
	if(lock) *lock = U1(_buf, 10);
	if(pacc) *pacc = U4(_buf, 24);
	if(pdop) *pdop = U2(_buf, 44);
	if(sats) *sats = U1(_buf, 47);
	
	return(GPS_OK);
}

int gps_get_dop(uint32_t *itow, uint16_t *gdop, uint16_t *pdop, uint16_t *tdop,
	uint16_t *vdop, uint16_t *hdop, uint16_t *ndop, uint16_t *edop)
{
	int r;
	
	/* Transmit the request and read response */
	gps_send_packet(UBX_CLASS_NAV, 0x04, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_NAV, 0x04, _buf, 18, 1500);
	if(r != GPS_OK) return(r);
	
	/* Parse response */
	if(itow) *itow = U4(_buf, 0);
	if(gdop) *gdop = U2(_buf, 4);
	if(pdop) *pdop = U2(_buf, 6);
	if(tdop) *tdop = U2(_buf, 8);
	if(vdop) *vdop = U2(_buf, 10);
	if(hdop) *hdop = U2(_buf, 12);
	if(ndop) *ndop = U2(_buf, 14);
	if(edop) *edop = U2(_buf, 16);
	
	return(GPS_OK);
}


int gps_set_nav(uint8_t nav)
{
	int r;
	
	/* Request the current navmode */
	gps_send_packet(UBX_CLASS_CFG, 0x24, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_CFG, 0x24, _buf, 36, 500);
	if(r != GPS_OK) return(r);
	
	/* The response is followed by an ACK-ACK */
	r = gps_get_ack(UBX_CLASS_CFG, 0x24, 500);
	if(r != GPS_OK) return(r);
	
	/* Set the new mode */
	_buf[2] = nav;
	
	/* Transmit the new setting */
	gps_send_packet(UBX_CLASS_CFG, 0x24, _buf, 36);
	r = gps_get_ack(UBX_CLASS_CFG, 0x24, 500);
	
	return(r);
}

int gps_get_nav(uint8_t *nav)
{
	int r;
	
	/* Transmit the request and read response */
	gps_send_packet(UBX_CLASS_CFG, 0x24, 0, 0);
	r = gps_get_packet_type(UBX_CLASS_CFG, 0x24, _buf, 36, 500);
	if(r != GPS_OK) return(r);
	
	/* Parse response */
	if(nav) *nav = _buf[2];
	
	/* The response is followed by an ACK-ACK */
	r = gps_get_ack(UBX_CLASS_CFG, 0x24, 500);
	
	return(GPS_OK);
}

int gps_set_psm(uint8_t psm)
{
	uint8_t cmd[] = { 0x08, 0x00 };
	int r;
	
	cmd[1] = psm;
	
	gps_send_packet(UBX_CLASS_CFG, 0x11, cmd, sizeof(cmd));
	r = gps_get_ack(UBX_CLASS_CFG, 0x11, 500);
	
	return(r);
}

