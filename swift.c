
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
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include "rtty.h"
#include "ax25modem.h"
#include "gps.h"
#include "geofence.h"

#define LEDBIT(b) PORTB = (PORTB & (~_BV(7))) | ((b) ? _BV(7) : 0)

void adc_init()
{
	/* Prepare the ADC */
	ADCSRA |= _BV(ADPS2) | _BV(ADPS1); /* /64 prescaler: 8Mhz = 125khz */
	ADMUX  |= _BV(REFS1); /* Internal 1.1V Voltage Reference */
	ADMUX  |= _BV(MUX0) | _BV(MUX2); /* ADC5 input */
	ADMUX  |= _BV(ADLAR); /* Left adjust the result */
	
	ADCSRA |= _BV(ADEN); /* Enable ADC */
}

uint16_t adc_read()
{
	uint16_t r;
	
	/* Start a single conversion */
	ADCSRA |= _BV(ADSC);
	
	/* Wait for the result */
	while(ADCSRA & _BV(ADSC));
	
	/* Read the result and convert to millivolts */
	r = ADCH * 100;
	r = r / 2 - r / 36;
	
	/* And return */
	return(r);
}

uint16_t crccat(char *msg)
{
	uint16_t x;
	for(x = 0xFFFF; *msg; msg++)
		x = _crc_xmodem_update(x, *msg);
	snprintf(msg, 8, "*%04X\n", x);
	return(x);
}

void tx_aprs(int32_t lat, int32_t lon, int32_t alt)
{
	char slat[5];
	char slng[5];
	char stlm[9];
	static uint16_t seq = 0;
	
	/* Convert the UBLOX-style coordinates to
	 * the APRS compressed format */
	lat = 900000000 - lat;
	lat = lat / 26 - lat / 2710 + lat / 15384615;
	
	lon = 900000000 + lon / 2;
	lon = lon / 26 - lon / 2710 + lon / 15384615;
	
	alt = alt / 1000 * 32808 / 10000;
	
	/* Construct the compressed telemetry format */
	ax25_base91enc(stlm + 0, 2, seq);
	ax25_base91enc(stlm + 2, 2, adc_read());
	
	ax25_frame(
		APRS_CALLSIGN, APRS_SSID,
		"APRS", 0,
		0, 0, 0, 0,
		//"WIDE1", 1,
		//"WIDE2", 1,
		"!/%s%sO   /A=%06ld|%s|",
		ax25_base91enc(slat, 4, lat),
		ax25_base91enc(slng, 4, lon),
		alt, stlm
	);
	
	if(seq % 60 == 0)
	{
		/* Transmit telemetry definitions */
		ax25_frame(
			APRS_CALLSIGN, APRS_SSID,
			"APRS", 0,
			0, 0, 0, 0,
			":%s-%i:PARM.Battery",
			APRS_CALLSIGN, APRS_SSID
		);
		ax25_frame(
			APRS_CALLSIGN, APRS_SSID,
			"APRS", 0,
			0, 0, 0, 0,
			":%s-%i :UNIT.mV",
			APRS_CALLSIGN, APRS_SSID
		);
	}
	
	seq++;
}

int main(void)
{
	uint32_t count = 0;
	int32_t lat, lon, alt;
	uint8_t hour, minute, second;
	uint16_t mv;
	char msg[100];
	
	/* Set the LED pin for output */
	DDRB |= _BV(DDB7);
	
	adc_init();
	rtx_init();
	ax25_init();
	gps_setup();
	
	sei();
	
	/* Enable the radio and let it settle */
	rtx_enable(1);
	_delay_ms(1000);
	
	rtx_string_P(PSTR(RTTY_CALLSIGN " starting up"));
	
	while(1)
	{
		if(!gps_get_pos(&lat, &lon, &alt))
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			continue;
		}
		
		if(!gps_get_time(&hour, &minute, &second))
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			continue;
		}
		
		/* Read the battery voltage */
		mv = adc_read();
		
		rtx_wait();
		
		snprintf(msg, 100, "$$%s,%li,%02i:%02i:%02i,%s%li.%05li,%s%li.%05li,%li,%i.%02i,%c",
			RTTY_CALLSIGN, count++,
			hour, minute, second,
			(lat < 0 ? "-" : ""), labs(lat) / 10000000, labs(lat) % 10000000 / 100,
			(lon < 0 ? "-" : ""), labs(lon) / 10000000, labs(lon) % 10000000 / 100,
			alt / 1000,
			mv / 1000, mv / 10 % 100,
			(geofence_test(lat, lon) ? '1' : '0'));
		crccat(msg + 2);
		rtx_string(msg);
		
#ifdef APRS_ENABLED
		tx_aprs(lat, lon, alt);
		{ int i; for(i = 0; i < 60; i++) _delay_ms(1000); }
#endif
	}
}

