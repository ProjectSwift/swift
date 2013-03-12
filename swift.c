
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
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include "rtty.h"
#include "ax25modem.h"
#include "gps.h"
#include "geofence.h"
#include "ds18x20.h"
#include "bmp085.h"
#include "c328.h"
#include "ssdv.h"

#define LEDBIT(b) PORTB = (PORTB & (~_BV(7))) | ((b) ? _BV(7) : 0)

uint8_t id[2][8];

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
	
	/* Skip initial '$'s */
	while(*msg == '$') msg++;
	
	/* Calculate the checksum */
	for(x = 0xFFFF; *msg; msg++)
		x = _crc_xmodem_update(x, *msg);
	
	/* Append it to the string */
	snprintf_P(msg, 8, PSTR("*%04X\n"), x);
	
	return(x);
}

#ifdef APRS_ENABLED
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
		char s[10];
		
		/* Make up the callsign */
		strncpy_P(s, PSTR(APRS_CALLSIGN), 7);
		if(APRS_SSID) snprintf_P(s + strlen(s), 4, PSTR("-%i"), APRS_SSID);
		
		/* Transmit telemetry definitions */
		ax25_frame(
			APRS_CALLSIGN, APRS_SSID,
			"APRS", 0,
			0, 0, 0, 0,
			":%-9s:PARM.Battery", s
		);
		ax25_frame(
			APRS_CALLSIGN, APRS_SSID,
			"APRS", 0,
			0, 0, 0, 0,
			":%-9s:UNIT.mV", s
		);
	}
	
	seq++;
}
#endif

#ifdef SSDV_ENABLED
char tx_image(void)
{
	static char setup = 0;
	static uint8_t img_id = 0;
	static ssdv_t ssdv;
	static uint8_t pkt[SSDV_PKT_SIZE];
	static uint8_t img[64];
	int r;
	
	if(!setup)
	{
		if((r = c3_open(SR_320x240)) != 0)
		{
			snprintf_P((char *) img, 64, PSTR("$$" RTTY_CALLSIGN ":Camera error %d\n"), r);
			rtx_string((char *) img);
			rtx_wait();
			return(setup);
		}
		
		setup = -1;
		
		ssdv_enc_init(&ssdv, RTTY_CALLSIGN, img_id++);
		ssdv_enc_set_buffer(&ssdv, pkt);
	}
	
	while((r = ssdv_enc_get_packet(&ssdv)) == SSDV_FEED_ME)
	{
		size_t r = c3_read(img, 64);
		if(r == 0) break;
		
		ssdv_enc_feed(&ssdv, img, r);
	}
	
	if(r != SSDV_OK)
	{
		/* Something went wrong! */
		c3_close();
		setup = 0;
		rtx_string_P(PSTR("$$" RTTY_CALLSIGN ":ssdv_enc_get_packet() failed\n"));
		return(setup);
	}
	
	if(ssdv.state == S_EOI || c3_eof())
	{
		/* The end of the image has been reached */
		c3_close();
		setup = 0;
	}
	
	/* Got the packet! Transmit it */
	rtx_data(pkt, SSDV_PKT_SIZE);
	
	return(setup);
}
#endif

int main(void)
{
	uint32_t count = 0;
	int32_t lat, lon, alt, temp1, temp2, pressure;
	uint8_t hour, minute, second;
	uint16_t mv;
	char msg[100];
	uint8_t i, r;
	bmp085_t bmp;
	
	/* Set the LED pin for output */
	DDRB |= _BV(DDB7);
	
	adc_init();
	rtx_init();
	ax25_init();
	bmp085_init(&bmp);
	
	sei();
	
	gps_setup();
	
	/* Enable the radio and let it settle */
	rtx_enable(1);
	_delay_ms(1000);
	
	rtx_string_P(PSTR(RTTY_CALLSIGN " starting up\n"));
	
	/* Scan the 1-wire bus, up to 16 devices */
	rtx_string_P(PSTR("Scanning 1-wire bus:\n"));
	for(i = 0; i < 16; i++)
	{
		r = ds_search_rom(id[i], i);
		
		if(r == DS_OK || r == DS_MORE)
		{
			/* A device was found, display the address */
			rtx_wait();
			snprintf_P(msg, 100, PSTR("%i> %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n"),
				i,
				id[i][0], id[i][1], id[i][2], id[i][3],
				id[i][4], id[i][5], id[i][6], id[i][7]);
				rtx_string(msg);
		}
		else
		{
			/* Device not responding or no devices found */
			rtx_wait();
			snprintf_P(msg, 100, PSTR("%i> Error %i\n"), i, r);
			rtx_string(msg);
		}
		
		/* No more devices? */
		if(r != DS_MORE) break;
	}
	rtx_string_P(PSTR("Done\n"));
	
	while(1)
	{
		/* Set the GPS navmode every 10 strings */
		if(count % 10 == 0)
		{
			/* Mode 6 is "Airborne with <1g Acceleration" */
			if(gps_set_nav(6) != GPS_OK)
			{
				rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",Error setting GPS navmode\n"));
			}
		}
		
		/* Get the latitude and longitude */
		if(gps_get_pos(&lat, &lon, &alt) != GPS_OK)
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			lat = lon = alt = 0;
		}
		
		/* Get the GPS time */
		if(gps_get_time(&hour, &minute, &second) != GPS_OK)
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			hour = minute = second = 0;
		}
		
		/* Read the battery voltage */
		mv = adc_read();
		
		/* Read the temperature from sensor 0 */
		ds_read_temperature(&temp1, id[0]);
		
		/* Read the temperature from sensor 1 */
		ds_read_temperature(&temp2, id[1]);
		
		/* Read the pressure from the BMP085 */
		if(bmp085_sample(&bmp, 3) != BMP_OK) pressure = 0;
		else pressure = bmp085_calc_pressure(&bmp);
		
		/* Build up the string */
		rtx_wait();
		
		snprintf_P(msg, 100, PSTR("$$%s,%li,%02i:%02i:%02i,%s%li.%04li,%s%li.%04li,%li,%i.%01i,%li,%li,%li,%c"),
			RTTY_CALLSIGN, count++,
			hour, minute, second,
			(lat < 0 ? "-" : ""), labs(lat) / 10000000, labs(lat) % 10000000 / 1000,
			(lon < 0 ? "-" : ""), labs(lon) / 10000000, labs(lon) % 10000000 / 1000,
			alt / 1000,
			mv / 1000, mv / 100 % 10,
			temp1 / 10000,
			temp2 / 10000,
			pressure,
			(geofence_test(lat, lon) ? '1' : '0'));
		crccat(msg);
		rtx_string(msg);
		
#ifdef APRS_ENABLED
		tx_aprs(lat, lon, alt);
		{ int i; for(i = 0; i < 60; i++) _delay_ms(1000); }
#endif

#ifdef SSDV_ENABLED
		if(tx_image() == -1)
		{
			/* The camera goes to sleep while transmitting telemetry,
			 * sync'ing here seems to prevent it. */
			c3_sync();
		}
#endif
	}
}

